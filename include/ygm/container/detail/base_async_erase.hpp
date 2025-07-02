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
 * async_erase operation for containers that searches for keys to erase in
 * containers with keys and values or values in containers without keys.
 *
 * @details Requires for_all_args to be a tuple with at least one item
 */
template <typename derived_type, typename for_all_args>
struct base_async_erase_key {
  /**
   * @brief Asynchronously erases a key from a container
   *
   * @param key Key to erase (key, value) pair of in containers with keys and
   * values or value to erase in containers without keys
   */
  void async_erase(const typename std::tuple_element<0, for_all_args>::type&
                       key) requires AtLeastOneItemTuple<for_all_args>

  {
    derived_type* derived_this = static_cast<derived_type*>(this);

    int dest = derived_this->partitioner.owner(key);

    auto updater =
        [](auto                                                      pcont,
           const typename std::tuple_element<0, for_all_args>::type& key) {
          pcont->local_erase(key);
        };

    derived_this->comm().async(dest, updater, derived_this->get_ygm_ptr(), key);
  }
};

/**
 * @brief Curiously-recurring template parameter struct that provides
 * async_erase operation for containers that searches for key and associated
 * values to erase in containers
 *
 * @details Requires for_all_args to be a tuple with two items
 */
template <typename derived_type, typename for_all_args>
struct base_async_erase_key_value {
  /**
   * @brief Asynchronously erases key and value from a container
   *
   * @param key Key to find in container
   * @param value Value to find associated to key
   *
   * @details Does nothing if (key, value) pair is not found.
   */
  void async_erase(
      const typename std::tuple_element<0, for_all_args>::type& key,
      const typename std::tuple_element<1, for_all_args>::type& value) requires
      DoubleItemTuple<for_all_args> {
    derived_type* derived_this = static_cast<derived_type*>(this);

    int dest = derived_this->partitioner.owner(key);

    auto updater =
        [](auto                                                      pcont,
           const typename std::tuple_element<0, for_all_args>::type& key,
           const typename std::tuple_element<1, for_all_args>::type& value) {
          pcont->local_erase(key, value);
        };

    derived_this->comm().async(dest, updater, derived_this->get_ygm_ptr(), key,
                               value);
  }
};

}  // namespace ygm::container::detail
