// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

namespace ygm::container::detail {

template <typename derived_type>
struct base_iterators {
  /**
   * @brief Returns an iterator to the beginning of the local container's data.
   * @warning The iterator is a local iterator, not a global iterator
   *
   * This function is primarly a convienience function for range based for loops
   * @return iterator to the beginning of the local container's data.
   */
  auto begin() {
    derived_type* derived_this = static_cast<derived_type*>(this);
    derived_this->comm().barrier();
    return derived_this->local_begin();
  }

  /**
   * @brief Returns a const_iterator to the beginning of the local container's
   * data.
   * @warning The const_iterator is a local iterator, not a global iterator
   *
   * This function is primarly a convienience function for range based for loops
   * @return auto const_iterator to the beginning of the local container's data.
   */
  auto begin() const {
    const derived_type* derived_this = static_cast<const derived_type*>(this);
    derived_this->comm().barrier();
    return derived_this->local_cbegin();
  }

  /**
   * @brief Returns a const_iterator to the beginning of the local container's
   * data.
   * @warning The const_iterator is a local iterator, not a global iterator
   *
   * This function is primarly a convienience function for range based for loops
   * @return auto const_iterator to the beginning of the local container's data.
   */
  auto cbegin() const {
    const derived_type* derived_this = static_cast<const derived_type*>(this);
    derived_this->comm().barrier();
    return derived_this->local_cbegin();
  }

  /**
   * @brief Returns an iterator to the end of the local container's data.
   * @warning The iterator is a local iterator, not a global iterator
   *
   * This function is primarly a convienience function for range based for loops
   * @return iterator to the end of the local container's data.
   */
  auto end() {
    derived_type* derived_this = static_cast<derived_type*>(this);
    derived_this->comm().barrier();
    return derived_this->local_end();
  }

  /**
   * @brief Returns a const_iterator to the end of the local container's data.
   * @warning The const_iterator is a local iterator, not a global iterator
   *
   * This function is primarly a convienience function for range based for loops
   * @return auto const_iterator to the end of the local container's data.
   */
  auto end() const {
    const derived_type* derived_this = static_cast<const derived_type*>(this);
    derived_this->comm().barrier();
    return derived_this->local_cend();
  }

  /**
   * @brief Returns a const_iterator to the end of the local container's data.
   * @warning The const_iterator is a local iterator, not a global iterator
   *
   * This function is primarly a convienience function for range based for loops
   * @return auto const_iterator to the end of the local container's data.
   */
  auto cend() const {
    const derived_type* derived_this = static_cast<const derived_type*>(this);
    derived_this->comm().barrier();
    return derived_this->local_cend();
  }
};

}  // namespace ygm::container::detail
