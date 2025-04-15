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
struct base_async_insert_value {
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

template <typename derived_type, typename for_all_args>
struct base_async_insert_key_value {
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

  void async_insert(
      const std::pair<const typename std::tuple_element<0, for_all_args>::type,
                      typename std::tuple_element<1, for_all_args>::type>&
          kvp) requires DoubleItemTuple<for_all_args> {
    async_insert(kvp.first, kvp.second);
  }
};

}  // namespace ygm::container::detail
