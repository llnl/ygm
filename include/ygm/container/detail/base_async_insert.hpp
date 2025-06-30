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
 * async_insert operation for containers that only contain values
 */
template <typename derived_type, typename for_all_args>
struct base_async_insert_value {
  /**
   * @brief Asynchronously inserts a value in a container
   *
   * @param value Value to insert into container
   *
   * \code{cpp}
   * ygm::container::bag<int> my_bag(world);
   * my_bag.async_insert(world.rank());
   * \endcode
   */
  void async_insert(const typename std::tuple_element<0, for_all_args>::type&
                        value) requires SingleItemTuple<for_all_args> {
    derived_type* derived_this = static_cast<derived_type*>(this);

    int dest = derived_this->partitioner.owner(value);

    auto inserter =
        [](auto                                                      pcont,
           const typename std::tuple_element<0, for_all_args>::type& item) {
          pcont->local_insert(item);
        };

    derived_this->comm().async(dest, inserter, derived_this->get_ygm_ptr(),
                               value);
  }
};

/**
 * @brief Curiously-recurring template parameter struct that provides
 * async_insert operation for containers that contain keys and values
 */
template <typename derived_type, typename for_all_args>
struct base_async_insert_key_value {
  /**
   * @brief Asynchronously insert a key-value pair into a container
   *
   * @param key Key to insert
   * @param value Value to associate to key
   * @details The container's local_insert() function is free to determine the
   * behavior when `key` is already in the container
   *
   * \code{cpp}
   *    ygm::container::map<int, std::string> my_map(world);
   *    my_map.async_insert(1, "one");
   * \endcode
   */
  void async_insert(
      const typename std::tuple_element<0, for_all_args>::type& key,
      const typename std::tuple_element<1, for_all_args>::type& value) requires
      DoubleItemTuple<for_all_args> {
    derived_type* derived_this = static_cast<derived_type*>(this);

    int dest = derived_this->partitioner.owner(key);

    auto inserter =
        [](auto                                                      pcont,
           const typename std::tuple_element<0, for_all_args>::type& key,
           const typename std::tuple_element<1, for_all_args>::type& value) {
          pcont->local_insert(key, value);
        };

    derived_this->comm().async(dest, inserter, derived_this->get_ygm_ptr(), key,
                               value);
  }

  /**
   * @brief Asynchronously insert a key-value pair into a container
   *
   * @param kvp Key-value pair to insert
   * @details Equivalent to `async_insert(kvp.first, kvp.second)`
   */
  void async_insert(
      const std::pair<const typename std::tuple_element<0, for_all_args>::type,
                      typename std::tuple_element<1, for_all_args>::type>&
          kvp) requires DoubleItemTuple<for_all_args> {
    async_insert(kvp.first, kvp.second);
  }
};

}  // namespace ygm::container::detail
