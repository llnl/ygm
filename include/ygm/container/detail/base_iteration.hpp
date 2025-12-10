// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <tuple>
#include <vector>
#include <ygm/container/detail/base_concepts.hpp>

namespace ygm::container::detail {

template <typename derived_type, typename FilterFunction>
class filter_proxy_value;
template <typename derived_type, typename FilterFunction>
class filter_proxy_key_value;

template <typename derived_type, typename TransformFunction>
class transform_proxy_value;
template <typename derived_type, typename TransformFunction>
class transform_proxy_key_value;

template <typename derived_type>
class flatten_proxy_value;
template <typename derived_type>
class flatten_proxy_key_value;

/**
 * @brief Curiously-recurring template parameter struct that provides
 * for_all, gather, gather_topk, reduce, collect, reduce_by_key, transform,
 * flatten, and filter operations to YGM containers without keys
 */
template <typename derived_type, SingleItemTuple for_all_args>
struct base_iteration_value {
  using value_type = typename std::tuple_element<0, for_all_args>::type;

  /**
   * @brief Iterates over all items in a container and executes a user-provided
   * function object on each.
   *
   * @tparam Function Type of user-provided function
   * @param fn User-provided function
   * @details The user-provided function is expected to take a single argument
   * that is an item within the container. If the user provides a lambda as
   * their function object, the lambda is allowed to capture.
   */
  template <typename Function>
  void for_all(Function&& fn) {
    auto* derived_this = static_cast<derived_type*>(this);
    derived_this->comm().barrier();
    derived_this->local_for_all(std::forward<Function>(fn));
  }

  /**
   * @brief Const version of for_all that iterates over all items and passes
   * them to the user function as const
   * *
   * @tparam Function Type of user-provided function
   * @param fn User-provided function
   * @details The user-provided function is expected to take a single argument
   * that is an item within the container. If the user provides a lambda as
   * their function object, the lambda is allowed to capture.
   */
  template <typename Function>
  void for_all(Function&& fn) const {
    const auto* derived_this = static_cast<const derived_type*>(this);
    derived_this->comm().barrier();
    derived_this->local_for_all(std::forward<Function>(fn));
  }

  /**
   * @brief Gather all values in an STL container
   *
   * @tparam STLContainer Type of STL container to gather to
   * @param gto Container to store results in
   * @param rank Rank to tather results on. Use -1 to gather to all ranks
   */
  template <typename STLContainer>
  void gather(STLContainer& gto, int rank) const {
    static_assert(
        std::is_same_v<typename STLContainer::value_type, value_type>);
    bool             all_gather   = (rank == -1);
    const auto*      derived_this = static_cast<const derived_type*>(this);
    const ygm::comm& mycomm       = derived_this->comm();
    [[maybe_unused]] static STLContainer*
        spgto;  // spgto is used, but compilers give warnings about it being
                // unused
    spgto = &gto;

    auto glambda = [&mycomm, rank, all_gather](const auto& value) {
      auto insert_lambda = [](const auto& value) {
        generic_insert(*spgto, value);
      };

      if (all_gather) {
        mycomm.async_bcast(insert_lambda, value);
      } else {
        mycomm.async(rank, insert_lambda, value);
      }
    };

    derived_this->for_all(glambda);

    derived_this->comm().barrier();
  }

  /**
   * @brief Gather all values in an STL container on all ranks
   *
   * @tparam STLContainer Type of STL container to gather to
   * @param gto Container to store results in
   * @details Equivalent to `gather(gto, -1)`
   */
  template <typename STLContainer>
  void gather(STLContainer& gto) const {
    gather(gto, -1);
  }

  /**
   * @brief Gather the k "largest" values according to provided comparison
   * function
   *
   * @tparam Compare Type of comparison operator
   * @param k Number of values to gather
   * @param comp Comparison function for identifying elements to gather
   * @return vector of largest values
   */
  template <typename Compare = std::greater<value_type>>
  std::vector<value_type> gather_topk(
      size_t k, Compare comp = std::greater<value_type>()) const
    requires SingleItemTuple<for_all_args>
  {
    const auto*      derived_this = static_cast<const derived_type*>(this);
    const ygm::comm& mycomm       = derived_this->comm();
    std::vector<value_type> local_topk;

    //
    // Find local top_k
    for_all([&local_topk, comp, k](const value_type& value) {
      local_topk.push_back(value);
      std::sort(local_topk.begin(), local_topk.end(), comp);
      if (local_topk.size() > k) {
        local_topk.pop_back();
      }
    });

    //
    // All reduce global top_k
    auto to_return = ::ygm::all_reduce(
        local_topk,
        [comp, k](const std::vector<value_type>& va,
                  const std::vector<value_type>& vb) {
          std::vector<value_type> out(va.begin(), va.end());
          out.insert(out.end(), vb.begin(), vb.end());
          std::sort(out.begin(), out.end(), comp);
          while (out.size() > k) {
            out.pop_back();
          }
          return out;
        },
        mycomm);
    return to_return;
  }

  /**
   * @brief Perform a reduction over all items in container
   *
   * @tparam MergeFunction Merge functor type
   * @param merge Functor to combine pairs of items
   * @return Value from all reductions
   * @details `reduce` only makes sense to use with commutative and associative
   * functors defining merges. Otherwise, ranks will not receive the same
   * result.
   */
  template <typename MergeFunction>
  value_type reduce(MergeFunction merge) const {
    const auto* derived_this = static_cast<const derived_type*>(this);
    derived_this->comm().barrier();

    using value_type = typename std::tuple_element<0, for_all_args>::type;
    bool first       = true;

    value_type local_reduce;

    auto rlambda = [&local_reduce, &first, &merge](const value_type& value) {
      if (first) {
        local_reduce = value;
        first        = false;
      } else {
        local_reduce = merge(local_reduce, value);
      }
    };

    derived_this->for_all(rlambda);

    std::optional<value_type> to_reduce;
    if (!first) {
      to_reduce = local_reduce;
    }

    std::optional<value_type> to_return =
        ::ygm::all_reduce(to_reduce, merge, derived_this->comm());
    YGM_ASSERT_RELEASE(to_return.has_value());
    return to_return.value();
  }

  /**
   * @brief Collects all items in a new YGM container
   *
   * @tparam YGMContainer Container type
   * @param c Container to collect into
   */
  template <typename YGMContainer>
  void collect(YGMContainer& c) const {
    const auto* derived_this = static_cast<const derived_type*>(this);
    auto clambda = [&c](const value_type& item) { c.async_insert(item); };
    derived_this->for_all(clambda);
  }

  /**
   * @brief Reduces all values in key-value pairs with matching keys
   *
   * @tparam MapType Result YGM container type
   * @tparam ReductionOp Functor type
   * @param map YGM container to hold result
   * @param reducer Functor for combining values
   */
  template <typename MapType, typename ReductionOp>
  void reduce_by_key(MapType& map, ReductionOp reducer) const {
    // TODO:  static_assert MapType is ygm::container::map
    const auto* derived_this = static_cast<const derived_type*>(this);
    using reduce_key_type    = typename MapType::key_type;
    using reduce_value_type  = typename MapType::mapped_type;
    static_assert(std::is_same_v<value_type,
                                 std::pair<reduce_key_type, reduce_value_type>>,
                  "value_type must be a std::pair");

    auto rbklambda =
        [&map, reducer](std::pair<reduce_key_type, reduce_value_type> kvp) {
          map.async_reduce(kvp.first, kvp.second, reducer);
        };
    derived_this->for_all(rbklambda);
  }

  template <typename TransformFunction>
  transform_proxy_value<derived_type, TransformFunction> transform(
      TransformFunction&& ffn);

  flatten_proxy_value<derived_type> flatten();

  template <typename FilterFunction>
  filter_proxy_value<derived_type, FilterFunction> filter(FilterFunction&& ffn);

 private:
  /**
   * @brief Generic function for insertion into STL containers, specialized for
   * using `push_back`
   *
   * @tparam STLContainer Container type
   * @tparam Value Type of value
   * @param stc STL container to insert into
   * @param v Value to insert
   */
  template <typename STLContainer, typename Value>
    requires requires(STLContainer stc, Value v) { stc.push_back(v); }
  static void generic_insert(STLContainer& stc, const Value& value) {
    stc.push_back(value);
  }

  /**
   * @brief Generic function for insertion into STL containers, specialized for
   * using `inseert`
   *
   * @tparam STLContainer Container type
   * @tparam Value Type of value
   * @param stc STL container to insert into
   * @param v Value to insert
   */
  template <typename STLContainer, typename Value>
    requires requires(STLContainer stc, Value v) { stc.insert(v); }
  static void generic_insert(STLContainer& stc, const Value& value) {
    stc.insert(value);
  }
};

/**
 * @brief Curiously-recurring template parameter struct that provides
 * for_all, gather, gather_topk, reduce, collect, reduce_by_key, transform,
 * flatten, and filter operations to YGM containers with keys and values
 *
 * @details Requires `for_all_args` to be a `tuple` of two items
 */
template <typename derived_type, DoubleItemTuple for_all_args>
struct base_iteration_key_value {
  using key_type    = typename std::tuple_element<0, for_all_args>::type;
  using mapped_type = typename std::tuple_element<1, for_all_args>::type;

  /**
   * @brief Iterates over all items in a container and executes a user-provided
   * function object on each.
   *
   * @tparam Function Type of user-provided function
   * @param fn User-provided function
   * @details The user-provided function is expected to take a single key and
   * value as separate arguments that make a (key, value) pair within the
   * container. If the user provides a lambda as their function object, the
   * lambda is allowed to capture.
   */
  template <typename Function>
  void for_all(Function fn) {
    auto* derived_this = static_cast<derived_type*>(this);
    derived_this->comm().barrier();
    derived_this->local_for_all(std::forward<Function>(fn));
  }

  /**
   * @brief Const version of for_all that iterates over all items and passes
   * them to the user function as const
   * *
   * @tparam Function Type of user-provided function
   * @param fn User-provided function
   * @details The user-provided function is expected to take a single key and
   * value as separate arguments that make a (key, value) pair within the
   * container. If the user provides a lambda as their function object, the
   * lambda is allowed to capture.
   */
  template <typename Function>
  void for_all(Function&& fn) const {
    const auto* derived_this = static_cast<const derived_type*>(this);
    derived_this->comm().barrier();
    derived_this->local_for_all(std::forward<Function>(fn));
  }

  /**
   * @brief Gather all values in an STL container
   *
   * @tparam STLContainer Type of STL container to gather to
   * @param gto Container to store results in
   * @param rank Rank to tather results on. Use -1 to gather to all ranks
   * @details Requires STL container to have a `value_type` that is (key, value)
   * pairs from the YGM container
   */
  template <typename STLContainer>
  void gather(STLContainer& gto, int rank) const {
    static_assert(std::is_same_v<typename STLContainer::value_type,
                                 std::pair<key_type, mapped_type>> ||
                  std::is_same_v<typename STLContainer::value_type,
                                 std::pair<const key_type, mapped_type>>);
    bool                 all_gather   = (rank == -1);
    const derived_type*  derived_this = static_cast<const derived_type*>(this);
    const ygm::comm&     mycomm       = derived_this->comm();
    static STLContainer* spgto;
    spgto = &gto;

    auto glambda = [&mycomm, rank, all_gather](const key_type&    key,
                                               const mapped_type& value) {
      auto insert_lambda = [](const key_type& key, const mapped_type& value) {
        generic_insert(*spgto, std::make_pair(key, value));
      };

      if (all_gather) {
        mycomm.async_bcast(insert_lambda, key, value);
      } else {
        mycomm.async(rank, insert_lambda, key, value);
      }
    };

    derived_this->for_all(glambda);

    derived_this->comm().barrier();
  }

  /**
   * @brief Gather all values in an STL container on all ranks
   *
   * @tparam STLContainer Type of STL container to gather to
   * @param gto Container to store results in
   * @details Equivalent to `gather(gto, -1)`
   */
  template <typename STLContainer>
  void gather(STLContainer& gto) const {
    gather(gto, -1);
  }

  /**
   * @brief Gather the k "largest" key-value pairs according to provided
   * comparison function
   *
   * @tparam Compare Type of comparison operator
   * @param k Number of key-value pairs to gather
   * @param comp Comparison function for identifying elements to gather
   * @return vector of largest key-value pairs
   */
  template <typename Compare = std::greater<std::pair<key_type, mapped_type>>>
  std::vector<std::pair<key_type, mapped_type>> gather_topk(
      size_t k, Compare comp = Compare()) const {
    const auto*      derived_this = static_cast<const derived_type*>(this);
    const ygm::comm& mycomm       = derived_this->comm();
    using vec_type = std::vector<std::pair<key_type, mapped_type>>;
    vec_type local_topk;

    //
    // Find local top_k
    for_all(
        [&local_topk, comp, k](const key_type& key, const mapped_type& mapped) {
          local_topk.push_back(std::make_pair(key, mapped));
          std::sort(local_topk.begin(), local_topk.end(), comp);
          if (local_topk.size() > k) {
            local_topk.pop_back();
          }
        });

    //
    // All reduce global top_k
    auto to_return = ::ygm::all_reduce(
        local_topk,
        [comp, k](const vec_type& va, const vec_type& vb) {
          vec_type out(va.begin(), va.end());
          out.insert(out.end(), vb.begin(), vb.end());
          std::sort(out.begin(), out.end(), comp);
          while (out.size() > k) {
            out.pop_back();
          }
          return out;
        },
        mycomm);
    return to_return;
  }

  /* Its unclear this makes sense for an associative container.
  template <typename MergeFunction>
  std::pair<key_type, mapped_type> reduce(MergeFunction merge) const {
    const derived_type* derived_this = static_cast<const derived_type*>(this);
    derived_this->comm().barrier();

    bool first = true;

    std::pair<key_type, mapped_type> local_reduce;

    auto rlambda = [&local_reduce, &first,
                    &merge](const std::pair<key_type, mapped_type>& value) {
      if (first) {
        local_reduce = value;
        first        = false;
      } else {
        local_reduce = merge(local_reduce, value);
      }
    };

    derived_this->for_all(rlambda);

    std::optional<std::pair<key_type, mapped_type>> to_reduce;
    if (!first) {  // local partition was empty!
      to_reduce = std::move(local_reduce);
    }

    std::optional<std::pair<key_type, mapped_type>> to_return =
        ::ygm::all_reduce(to_reduce, merge, derived_this->comm());
    YGM_ASSERT_RELEASE(to_return.has_value());
    return to_return.value();
  }
  */

  /**
   * @brief Collects all items in a new YGM container
   *
   * @tparam YGMContainer Container type
   * @param c Container to collect into
   */
  template <typename YGMContainer>
  void collect(YGMContainer& c) const {
    const auto* derived_this = static_cast<const derived_type*>(this);
    auto        clambda = [&c](const key_type& key, const mapped_type& value) {
      c.async_insert(std::make_pair(key, value));
    };
    derived_this->for_all(clambda);
  }

  /**
   * @brief Reduces all values in key-value pairs with matching keys
   *
   * @tparam MapType Result YGM container type
   * @tparam ReductionOp Functor type
   * @param map YGM container to hold result
   * @param reducer Functor for combining values
   */
  template <typename MapType, typename ReductionOp>
  void reduce_by_key(MapType& map, ReductionOp reducer) const {
    const auto* derived_this = static_cast<const derived_type*>(this);
    // static_assert ygm::map
    using reduce_key_type   = typename MapType::key_type;
    using reduce_value_type = typename MapType::mapped_type;

    static_assert(std::tuple_size<for_all_args>::value == 2);
    auto rbklambda = [&map, reducer](const reduce_key_type&   key,
                                     const reduce_value_type& value) {
      map.async_reduce(key, value, reducer);
    };
    derived_this->for_all(rbklambda);
  }

  template <typename TransformFunction>
  transform_proxy_key_value<derived_type, TransformFunction> transform(
      TransformFunction&& ffn);

  /**
   * @brief Access to container presenting only keys
   *
   * @return Transform object that returns only keys to user
   */
  auto keys() {
    return transform([](const key_type&                     key,
                        [[maybe_unused]] const mapped_type& value) -> key_type {
      return key;
    });
  }

  /**
   * @brief Access to container presenting only values
   *
   * @return Transform object that returns only values to user
   */
  auto values() {
    return transform(
        []([[maybe_unused]] const key_type& key,
           const mapped_type& value) -> mapped_type { return value; });
  }

  flatten_proxy_key_value<derived_type> flatten();

  template <typename FilterFunction>
  filter_proxy_key_value<derived_type, FilterFunction> filter(
      FilterFunction&& ffn);

 private:
  /**
   * @brief Generic function for insertion into STL containers, specialized for
   * using `push_back`
   *
   * @tparam STLContainer Container type
   * @tparam Value Type of value
   * @param stc STL container to insert into
   * @param v Value to insert
   */
  template <typename STLContainer, typename Value>
    requires requires(STLContainer stc, Value v) { stc.push_back(v); }
  static void generic_insert(STLContainer& stc, const Value& value) {
    stc.push_back(value);
  }

  /**
   * @brief Generic function for insertion into STL containers, specialized for
   * using `inseert`
   *
   * @tparam STLContainer Container type
   * @tparam Value Type of value
   * @param stc STL container to insert into
   * @param v Value to insert
   */
  template <typename STLContainer, typename Value>
    requires requires(STLContainer stc, Value v) { stc.insert(v); }
  static void generic_insert(STLContainer& stc, const Value& value) {
    stc.insert(value);
  }
};

}  // namespace ygm::container::detail
#include <ygm/container/detail/filter_proxy.hpp>
#include <ygm/container/detail/flatten_proxy.hpp>
#include <ygm/container/detail/transform_proxy.hpp>

namespace ygm::container::detail {

/**
 * @brief Creates proxy that transforms items in container that are presented to
 * user `for_all` calls
 *
 * @tparam TransformFunction functor type
 * @param ffn Function to transform items in container
 * @details The underlying items within the container are not modified.
 *
 * \code{cpp}
 * ygm::container::bag<int> my_bag(world);
 * my_bag.async_insert(2);
 * my_bag.barrier();
 *
 * my_bag.transform([](auto &val) { return 2*val; }).for_all([](const auto
 * &transformed_val) { YGM_ASSERT_RELEASE(val == 4);
 * });
 *
 * my_bag.for_all([](const auto &val) { YGM_ASSERT_RELEASE(val == 2); });
 * \endcode
 * will complete successfully.
 */
template <typename derived_type, SingleItemTuple for_all_args>
template <typename TransformFunction>
transform_proxy_value<derived_type, TransformFunction>
base_iteration_value<derived_type, for_all_args>::transform(
    TransformFunction&& ffn) {
  auto* derived_this = static_cast<derived_type*>(this);
  return transform_proxy_value<derived_type, TransformFunction>(
      *derived_this, std::forward<TransformFunction>(ffn));
}

/**
 * @brief Flattens STL containers of values to allow a function to be called on
 * inner items individually
 *
 * @details Underlying container is not modified.
 *
 * \code{cpp}
 * ygm::container::bag<std::vector<int>> my_bag(world, {{1, 2, 3}});
 *
 * my_bag.flatten().for_all([](const int &nested_val) {
 * std::cout << "Nested value: " << nested_val << std::cout;
 * });
 * \endcode
 * will print
 * ```
 * Nested value: 1
 * Nested value: 2
 * Nested value: 3
 * ```
 */
template <typename derived_type, SingleItemTuple for_all_args>
inline flatten_proxy_value<derived_type>
base_iteration_value<derived_type, for_all_args>::flatten() {
  // static_assert(
  //     type_traits::is_vector<std::tuple_element<0, for_all_args>>::value);
  auto* derived_this = static_cast<derived_type*>(this);
  return flatten_proxy_value<derived_type>(*derived_this);
}

/**
 * @brief Filters items in a container so only allow `for_all` to execute on
 * those that satisfy a given predicate function.
 *
 * @tparam FilterFunction Functor type
 * @param ffn Function used to filter items in container.
 * @details Filtered items are not removed from underlying container.
 *
 * \code{cpp}
 * ygm::container::bag<int> my_bag(world, {1, 2, 3, 4});
 * my_bag.filter([](const auto &val) { return (val % 2) == 0;
 * }).for_all([](const auto &filtered_val) { YGM_ASSERT_RELEASE((filtered_val %
 * 2) == 0);
 * });
 * \endcode
 */
template <typename derived_type, SingleItemTuple for_all_args>
template <typename FilterFunction>
filter_proxy_value<derived_type, FilterFunction>
base_iteration_value<derived_type, for_all_args>::filter(FilterFunction&& ffn) {
  auto* derived_this = static_cast<derived_type*>(this);
  return filter_proxy_value<derived_type, FilterFunction>(
      *derived_this, std::forward<FilterFunction>(ffn));
}

/**
 * @brief Creates proxy that transforms key-value pairs in a
 * container that are presented to user `for_all` calls
 *
 * @tparam TransformFunction functor type
 * @param ffn Function to transform items in container
 * @details The underlying items within the container are not modified.
 *
 */
template <typename derived_type, DoubleItemTuple for_all_args>
template <typename TransformFunction>
transform_proxy_key_value<derived_type, TransformFunction>
base_iteration_key_value<derived_type, for_all_args>::transform(
    TransformFunction&& ffn) {
  auto* derived_this = static_cast<derived_type*>(this);
  return transform_proxy_key_value<derived_type, TransformFunction>(
      *derived_this, std::forward<TransformFunction>(ffn));
}

/**
 * @brief Flattens STL containers of values to allow a function to be called on
 * inner items individually
 *
 * @details Underlying container is not modified.
 */
template <typename derived_type, DoubleItemTuple for_all_args>
inline flatten_proxy_key_value<derived_type>
base_iteration_key_value<derived_type, for_all_args>::flatten() {
  // static_assert(
  //     type_traits::is_vector<std::tuple_element<0, for_all_args>>::value);
  auto* derived_this = static_cast<derived_type*>(this);
  return flatten_proxy_key_value<derived_type>(*derived_this);
}

/**
 * @brief Filters items in a container so only allow `for_all` to execute on
 * those that satisfy a given predicate function.
 *
 * @tparam FilterFunction Functor type
 * @param ffn Function used to filter items in container.
 * @details Filtered items are not removed from underlying container.
 */
template <typename derived_type, DoubleItemTuple for_all_args>
template <typename FilterFunction>
filter_proxy_key_value<derived_type, FilterFunction>
base_iteration_key_value<derived_type, for_all_args>::filter(
    FilterFunction&& ffn) {
  auto* derived_this = static_cast<derived_type*>(this);
  return filter_proxy_key_value<derived_type, FilterFunction>(
      *derived_this, std::forward<FilterFunction>(ffn));
}

}  // namespace ygm::container::detail
