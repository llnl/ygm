// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

namespace ygm::container::detail {

/**
 * @brief Curiously-recurring template parameter struct that provides
 * size, clear, swap, comm, and get_ygm_ptr operations
 */
template <typename derived_type, typename for_all_args>
struct base_misc {
  /**
   * @brief Gets number of elements in a YGM container
   *
   * @return Container size
   */
  size_t size() const {
    const derived_type* derived_this = static_cast<const derived_type*>(this);
    derived_this->comm().barrier();
    return ::ygm::sum(derived_this->local_size(), derived_this->comm());
  }

  /**
   * @brief Clears the contents of a YGM container
   */
  void clear() {
    derived_type* derived_this = static_cast<derived_type*>(this);
    derived_this->comm().barrier();
    derived_this->local_clear();
  }

  /**
   * @brief Swaps the contents of a YGM container
   *
   * @param other Container to swap with
   */
  void swap(derived_type& other) {
    derived_type* derived_this = static_cast<derived_type*>(this);
    derived_this->comm().barrier();
    derived_this->local_swap(other);
  }

  /**
   * @brief Access to underlying YGM communicator
   *
   * @return YGM communicator used for communication by container
   */
  ygm::comm& comm() const {
    return static_cast<const derived_type*>(this)->m_comm;
  }

  /**
   * @brief Access to the ygm_ptr used by the container
   *
   * @return `ygm_ptr` used by the container when identifying itself in `async`
   * calls on the `ygm::comm`
   */
  typename ygm::ygm_ptr<derived_type> get_ygm_ptr() {
    return static_cast<derived_type*>(this)->pthis;
  }

  /**
   * @brief Const access to the ygm ptr used by the container
   *
   * @return `ygm_ptr` to const version of container
   */
  const typename ygm::ygm_ptr<derived_type> get_ygm_ptr() const {
    return static_cast<const derived_type*>(this)->pthis;
  }
};

}  // namespace ygm::container::detail
