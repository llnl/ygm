// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <tuple>
#include <utility>
#include <ygm/container/detail/base_concepts.hpp>

namespace ygm::container::detail {

/**
 * @brief Curiously-recurring template parameter struct that provides
 * async_insert_or_assign operation for containers with keys and values.
 */
template <typename derived_type, typename for_all_args>
struct base_async_insert_or_assign {
  /**
   * @brief Asynchronously insert `(key, value)` pair into container if it does
   * not already exist or assign `value` to `key` if `key` already exists in the
   * container
   *
   * @param key Key to attempt insertion of
   * @param value Value to associate with key
   * @details Behavior is meant to mirror `std::map::insert_or_assign`
   */
  void async_insert_or_assign(
      const typename std::tuple_element<0, for_all_args>::type& key,
      const typename std::tuple_element<1, for_all_args>::type& value) requires
      DoubleItemTuple<for_all_args> {
    derived_type* derived_this = static_cast<derived_type*>(this);

    int dest = derived_this->partitioner.owner(key);

    auto updater =
        [](auto                                                      pcont,
           const typename std::tuple_element<0, for_all_args>::type& key,
           const typename std::tuple_element<1, for_all_args>::type& value) {
          pcont->local_insert_or_assign(key, value);
        };

    derived_this->comm().async(dest, updater, derived_this->get_ygm_ptr(), key,
                               value);
  }

  /**
   * @brief Asynchronously insert `(key, value)` pair into container if it does
   * not already exist or assign `value` to `key` if `key` already exists in the
   * container
   *
   * @param kvp Key-value pair to attempt to insert
   * @details Equivalent to `async_insert_or_assign(kvp.first, kvp.second)`
   */
  void async_insert_or_assign(
      const std::pair<typename std::tuple_element<0, for_all_args>::type,
                      typename std::tuple_element<1, for_all_args>::type>&
          kvp) requires DoubleItemTuple<for_all_args> {
    async_insert_or_assign(kvp.first, kvp.second);
  }
};

}  // namespace ygm::container::detail
