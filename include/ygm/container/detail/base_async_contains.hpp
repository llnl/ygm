// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <tuple>
#include <utility>
#include <ygm/detail/lambda_compliance.hpp>
#include <ygm/detail/meta/functional.hpp>

namespace ygm::container::detail {

/**
 * @brief Curiously-recurring template parameter struct that provides
 * async_contains operation
 */
template <typename derived_type, typename for_all_args>
struct base_async_contains {
  /**
   * @brief Asynchronously execute a function with knowledge of if the container
   * contains the given value
   *
   * @tparam Function Type of user function
   * @tparam FuncArgs... Variadic types of user-provided arguments to function
   * @param value Value to check for existence of in container
   * @param fn User-provided function to execute
   * @param args... Variadic arguments to user-provided function
   * @details The user-provided function is provided with (1) an optional
   * pointer to the container, (2) a boolean indicating whether the desired
   * value was found, (3) the value searched for, and (4) any additional
   * arguments passed to async_contains by the user
   */
  template <typename Function, typename... FuncArgs>
  void async_contains(
      const typename std::tuple_element<0, for_all_args>::type& value,
      Function&& fn, const FuncArgs&... args) {
    YGM_CHECK_ASYNC_LAMBDA_COMPLIANCE(Function,
                                      "ygm::container::async_contains()");

    derived_type* derived_this = static_cast<derived_type*>(this);

    int dest = derived_this->partitioner.owner(value);

    auto lambda =
        [fn](auto                                                      pcont,
             const typename std::tuple_element<0, for_all_args>::type& value,
             const FuncArgs&... args) mutable {
          bool contains = static_cast<bool>(pcont->local_count(value));
          ygm::meta::apply_optional(
              std::forward<Function>(fn), std::make_tuple(pcont),
              std::forward_as_tuple(contains, value, args...));
        };

    derived_this->comm().async(dest, lambda, derived_this->get_ygm_ptr(), value,
                               args...);
  }
};

}  // namespace ygm::container::detail
