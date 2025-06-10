// Copyright 2019-2021 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

namespace ygm::container::detail {

template <typename derived_type, typename for_all_args>
struct base_misc {
  size_t size() const {
    const derived_type* derived_this = static_cast<const derived_type*>(this);
    derived_this->comm().barrier();
    return ::ygm::sum(derived_this->local_size(), derived_this->comm());
  }

  void clear() {
    derived_type* derived_this = static_cast<derived_type*>(this);
    derived_this->comm().barrier();
    derived_this->local_clear();
  }

  void swap(derived_type& other) {
    derived_type* derived_this = static_cast<derived_type*>(this);
    derived_this->comm().barrier();
    derived_this->local_swap(other);
  }

  ygm::comm& comm() const {
    return static_cast<const derived_type*>(this)->m_comm;
  }

  typename ygm::ygm_ptr<derived_type> get_ygm_ptr() {
    return static_cast<derived_type*>(this)->pthis;
  }

  const typename ygm::ygm_ptr<derived_type> get_ygm_ptr() const {
    return static_cast<const derived_type*>(this)->pthis;
  }
};

}  // namespace ygm::container::detail
