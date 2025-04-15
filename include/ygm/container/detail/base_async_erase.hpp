// Copyright 2019-2021 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <tuple>
#include <utility>
#include <ygm/container/detail/base_concepts.hpp>

namespace ygm::container::detail {

template <typename derived_type, typename for_all_args>
struct base_async_erase_key {
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

template <typename derived_type, typename for_all_args>
struct base_async_erase_key_value {
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
