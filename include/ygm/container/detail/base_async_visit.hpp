// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <tuple>
#include <utility>
#include <ygm/container/detail/base_concepts.hpp>
#include <ygm/detail/interrupt_mask.hpp>
#include <ygm/detail/lambda_compliance.hpp>

namespace ygm::container::detail {

/**
 * @brief Curiously-recurring template parameter struct that provides
 * async_visit operation to containers with key-value pairs
 */
template <typename derived_type, typename for_all_args>
struct base_async_visit {
  /**
   * @brief Asynchronously visit key within a container and execute a
   * user-provided function
   *
   * @tparam Visitor Type of user-provided function
   * @tparam VisitorArgs... Variadic argument types to give to user function
   * @param key Key to visit in container
   * @param visitor User-provided function to execute at key
   * @param args... Variadic arguments to pass to user-provided function
   *
   * \code{cpp}
   * ygm::container::map<std::string, int> my_map(world);
   * my_map.async_insert("one", 1);
   * world.barrier();
   * my_map.async_visit("one", [](const auto &key, auto &val, int &to_add) {
   *    val += to_add;
   *    }, world.size())
   * world.barrier();
   * \endcode
   * will result in a value of `world.size() * world.size() + 1` associated to
   * the key `"one"`.
   */
  template <typename Visitor, typename... VisitorArgs>
  void async_visit(
      const typename std::tuple_element<0, for_all_args>::type& key,
      Visitor&&                                                 visitor,
      const VisitorArgs&... args) requires DoubleItemTuple<for_all_args> {
    YGM_CHECK_ASYNC_LAMBDA_COMPLIANCE(Visitor, "ygm::container::async_visit()");

    derived_type* derived_this = static_cast<derived_type*>(this);

    int dest = derived_this->partitioner.owner(key);

    // Copy of visitor is created when capturing. If this capture is done by
    // reference, then the async call will serialize a reference to visitor
    // (including its primitive captures), rather than copying visitor itself
    auto vlambda =
        [visitor](auto pcont,
                  const typename std::tuple_element<0, for_all_args>::type& key,
                  const VisitorArgs&... args) mutable {
          pcont->local_visit(key, std::forward<Visitor>(visitor), args...);
        };

    derived_this->comm().async(dest, vlambda, derived_this->get_ygm_ptr(), key,
                               args...);
  }

  /**
   * @brief Asynchronously visit key within a container and execute a
   * user-provided function only if the key already exists
   *
   * @tparam Visitor Type of user-provided function
   * @tparam VisitorArgs... Variadic argument types to give to user function
   * @param key Key to visit in container
   * @param visitor User-provided function to execute at key
   * @param args... Variadic arguments to pass to user-provided function
   * @details This function differs from `async_visit` in that it will not
   * default construct a value within the container prior to visiting a key that
   * does not already exist.
   */
  template <typename Visitor, typename... VisitorArgs>
  void async_visit_if_contains(
      const typename std::tuple_element<0, for_all_args>::type& key,
      Visitor                                                   visitor,
      const VisitorArgs&... args) requires DoubleItemTuple<for_all_args> {
    YGM_CHECK_ASYNC_LAMBDA_COMPLIANCE(
        Visitor, "ygm::container::async_visit_if_contains()");

    derived_type* derived_this = static_cast<derived_type*>(this);

    int dest = derived_this->partitioner.owner(key);

    // Copy of visitor is created when capturing. If this capture is done by
    // reference, then the async call will serialize a reference to visitor
    // (including its primitive captures), rather than copying visitor itself
    auto vlambda =
        [visitor](auto pcont,
                  const typename std::tuple_element<0, for_all_args>::type& key,
                  const VisitorArgs&... args) mutable {
          pcont->local_visit_if_contains(key, visitor, args...);
        };

    derived_this->comm().async(dest, vlambda, derived_this->get_ygm_ptr(), key,
                               args...);
  }

  /**
   * @brief Version of `async_visit_if_contains` that works on `const` objects
   * and provides `const` arguments to the user-provided lambda
   */
  template <typename Visitor, typename... VisitorArgs>
  void async_visit_if_contains(
      const typename std::tuple_element<0, for_all_args>::type& key,
      Visitor                                                   visitor,
      const VisitorArgs&... args) const requires DoubleItemTuple<for_all_args> {
    YGM_CHECK_ASYNC_LAMBDA_COMPLIANCE(
        Visitor, "ygm::container::async_visit_if_contains()");

    const derived_type* derived_this = static_cast<const derived_type*>(this);

    int dest = derived_this->partitioner.owner(key);

    // Copy of visitor is created when capturing. If this capture is done by
    // reference, then the async call will serialize a reference to visitor
    // (including its primitive captures), rather than copying visitor itself
    auto vlambda =
        [visitor](const auto pcont,
                  const typename std::tuple_element<0, for_all_args>::type& key,
                  const VisitorArgs&... args) mutable {
          pcont->local_visit_if_contains(key, visitor, args...);
        };

    derived_this->comm().async(dest, vlambda, derived_this->get_ygm_ptr(), key,
                               args...);
  }

  // todo:   async_insert_visit()
};

}  // namespace ygm::container::detail
