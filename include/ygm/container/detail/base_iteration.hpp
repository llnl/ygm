// Copyright 2019-2021 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <tuple>
#include <vector>
#include <ygm/collective.hpp>
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

template <typename derived_type, SingleItemTuple for_all_args>
struct base_iteration_value {
  using value_type = typename std::tuple_element<0, for_all_args>::type;

  template <typename Function>
  void for_all(Function&& fn) {
    auto* derived_this = static_cast<derived_type*>(this);
    derived_this->comm().barrier();
    derived_this->local_for_all(std::forward<Function>(fn));
  }

  template <typename Function>
  void for_all(Function&& fn) const {
    const auto* derived_this = static_cast<const derived_type*>(this);
    derived_this->comm().barrier();
    derived_this->local_for_all(std::forward<Function>(fn));
  }

  template <typename STLContainer>
  void gather(STLContainer& gto, int rank) const {
    static_assert(
        std::is_same_v<typename STLContainer::value_type, value_type>);
    bool                 all_gather   = (rank == -1);
    const auto*          derived_this = static_cast<const derived_type*>(this);
    const ygm::comm&     mycomm       = derived_this->comm();
    static STLContainer* spgto;
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

  template <typename STLContainer>
  void gather(STLContainer& gto) const {
    gather(gto, -1);
  }

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
    auto to_return = mycomm.all_reduce(
        local_topk, [comp, k](const std::vector<value_type>& va,
                              const std::vector<value_type>& vb) {
          std::vector<value_type> out(va.begin(), va.end());
          out.insert(out.end(), vb.begin(), vb.end());
          std::sort(out.begin(), out.end(), comp);
          while (out.size() > k) {
            out.pop_back();
          }
          return out;
        });
    return to_return;
  }

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

  template <typename YGMContainer>
  void collect(YGMContainer& c) const {
    const auto* derived_this = static_cast<const derived_type*>(this);
    auto clambda = [&c](const value_type& item) { c.async_insert(item); };
    derived_this->for_all(clambda);
  }

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
  template <typename STLContainer, typename Value>
    requires requires(STLContainer stc, Value v) { stc.push_back(v); }
  static void generic_insert(STLContainer& stc, const Value& value) {
    stc.push_back(value);
  }

  template <typename STLContainer, typename Value>
    requires requires(STLContainer stc, Value v) { stc.insert(v); }
  static void generic_insert(STLContainer& stc, const Value& value) {
    stc.insert(value);
  }
};

// For Associative Containers
template <typename derived_type, DoubleItemTuple for_all_args>
struct base_iteration_key_value {
  using key_type    = typename std::tuple_element<0, for_all_args>::type;
  using mapped_type = typename std::tuple_element<1, for_all_args>::type;

  template <typename Function>
  void for_all(Function fn) {
    auto* derived_this = static_cast<derived_type*>(this);
    derived_this->comm().barrier();
    derived_this->local_for_all(std::forward<Function>(fn));
  }

  template <typename Function>
  void for_all(Function&& fn) const {
    const auto* derived_this = static_cast<const derived_type*>(this);
    derived_this->comm().barrier();
    derived_this->local_for_all(std::forward<Function>(fn));
  }

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

  template <typename STLContainer>
  void gather(STLContainer& gto) const {
    gather(gto, -1);
  }

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
    auto to_return = mycomm.all_reduce(
        local_topk, [comp, k](const vec_type& va, const vec_type& vb) {
          vec_type out(va.begin(), va.end());
          out.insert(out.end(), vb.begin(), vb.end());
          std::sort(out.begin(), out.end(), comp);
          while (out.size() > k) {
            out.pop_back();
          }
          return out;
        });
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

  template <typename YGMContainer>
  void collect(YGMContainer& c) const {
    const auto* derived_this = static_cast<const derived_type*>(this);
    auto        clambda = [&c](const key_type& key, const mapped_type& value) {
      c.async_insert(std::make_pair(key, value));
    };
    derived_this->for_all(clambda);
  }

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

  auto keys() {
    return transform([](const key_type&    key,
                        const mapped_type& value) -> key_type { return key; });
  }

  auto values() {
    return transform(
        [](const key_type& key, const mapped_type& value) -> mapped_type {
          return value;
        });
  }

  flatten_proxy_key_value<derived_type> flatten();

  template <typename FilterFunction>
  filter_proxy_key_value<derived_type, FilterFunction> filter(
      FilterFunction&& ffn);

 private:
  template <typename STLContainer, typename Value>
    requires requires(STLContainer stc, Value v) { stc.push_back(v); }
  static void generic_insert(STLContainer& stc, const Value& value) {
    stc.push_back(value);
  }

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

template <typename derived_type, SingleItemTuple for_all_args>
template <typename TransformFunction>
transform_proxy_value<derived_type, TransformFunction>
base_iteration_value<derived_type, for_all_args>::transform(
    TransformFunction&& ffn) {
  auto* derived_this = static_cast<derived_type*>(this);
  return transform_proxy_value<derived_type, TransformFunction>(
      *derived_this, std::forward<TransformFunction>(ffn));
}

template <typename derived_type, SingleItemTuple for_all_args>
inline flatten_proxy_value<derived_type>
base_iteration_value<derived_type, for_all_args>::flatten() {
  // static_assert(
  //     type_traits::is_vector<std::tuple_element<0, for_all_args>>::value);
  auto* derived_this = static_cast<derived_type*>(this);
  return flatten_proxy_value<derived_type>(*derived_this);
}

template <typename derived_type, SingleItemTuple for_all_args>
template <typename FilterFunction>
filter_proxy_value<derived_type, FilterFunction>
base_iteration_value<derived_type, for_all_args>::filter(FilterFunction&& ffn) {
  auto* derived_this = static_cast<derived_type*>(this);
  return filter_proxy_value<derived_type, FilterFunction>(
      *derived_this, std::forward<FilterFunction>(ffn));
}

template <typename derived_type, DoubleItemTuple for_all_args>
template <typename TransformFunction>
transform_proxy_key_value<derived_type, TransformFunction>
base_iteration_key_value<derived_type, for_all_args>::transform(
    TransformFunction&& ffn) {
  auto* derived_this = static_cast<derived_type*>(this);
  return transform_proxy_key_value<derived_type, TransformFunction>(
      *derived_this, std::forward<TransformFunction>(ffn));
}

template <typename derived_type, DoubleItemTuple for_all_args>
inline flatten_proxy_key_value<derived_type>
base_iteration_key_value<derived_type, for_all_args>::flatten() {
  // static_assert(
  //     type_traits::is_vector<std::tuple_element<0, for_all_args>>::value);
  auto* derived_this = static_cast<derived_type*>(this);
  return flatten_proxy_key_value<derived_type>(*derived_this);
}

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
