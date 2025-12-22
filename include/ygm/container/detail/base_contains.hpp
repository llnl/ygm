// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <tuple>
#include <utility>

namespace ygm::container::detail {

/**
 * @brief Curiously-recurring template parameter struct that provides
 * count operation
 */
template <typename derived_type, typename for_all_args>
struct base_contains {
  /**
   * @brief Checks for the presence of a value within a container.
   *
   * @param value Value to search for within container (key in the case of
   * containers with keys)
   * @return True if `value` exists in container; false otherwise.
   */
  bool contains(
      const typename std::tuple_element<0, for_all_args>::type& value) const {
    const derived_type* derived_this = static_cast<const derived_type*>(this);
    derived_this->comm().barrier();
    return ::ygm::logical_or(derived_this->local_contains(value),
                             derived_this->comm());
  }
};

}  // namespace ygm::container::detail
