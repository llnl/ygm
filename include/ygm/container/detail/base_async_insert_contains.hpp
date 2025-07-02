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
 * async_insert_contains operation
 */
template <typename derived_type, typename for_all_args>
struct base_async_insert_contains {
  /**
   * @brief Asynchronously insert into a container if value is not already
   * present and execute a user-provided function that is told whether the value
   * was already present
   *
   * @param value Value to attempt to insert
   * @param fn Function to execute after attempted insertion
   * @param args... Variadic arguments to pass to fn
   * @details Insertion only occurs if value is not already present. Containers
   * with keys and values will not have values reset to the value's default.
   *
   * \code{cpp}
   * ygm::container::map<int, int> my_map(world);
   * my_bag.async_insert_contains(10, [](bool contains, auto &value) {
   *    if (contains) {
   *      wcout() << "my_map already contained " << value << std::endl;
   *    } else {
   *      wcout() << "my_map did not already contain " << value << " but now it
   * does" << std::endl;
   *    }
   *  });
   *  \endcode
   */
  template <typename Function, typename... FuncArgs>
  void async_insert_contains(
      const typename std::tuple_element<0, for_all_args>::type& value,
      Function&& fn, const FuncArgs&... args) {
    YGM_CHECK_ASYNC_LAMBDA_COMPLIANCE(
        Function, "ygm::container::async_insert_contains()");

    derived_type* derived_this = static_cast<derived_type*>(this);

    int dest = derived_this->partitioner.owner(value);

    auto lambda =
        [fn](auto                                                      pcont,
             const typename std::tuple_element<0, for_all_args>::type& value,
             const FuncArgs&... args) mutable {
          bool contains = static_cast<bool>(pcont->local_count(value));
          if (!contains) {
            pcont->local_insert(value);
          }

          ygm::meta::apply_optional(
              std::forward<Function>(fn), std::make_tuple(pcont),
              std::forward_as_tuple(contains, value, args...));
        };

    derived_this->comm().async(dest, lambda, derived_this->get_ygm_ptr(), value,
                               args...);
  }
};

}  // namespace ygm::container::detail
