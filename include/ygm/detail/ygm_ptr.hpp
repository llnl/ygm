// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <vector>
#include <ygm/detail/collective.hpp>
#include <ygm/utility/assert.hpp>

namespace ygm {

template <typename T>
class ygm_ptr {
 public:
  ygm_ptr() {};

  T *operator->() const { return sptrs[idx]; }

  T &operator*() const { return *sptrs[idx]; }

  /**
   * @brief Construct a new ygm ptr object
   *
   * @warning The user is responsible for ensuring all processes have completed
   * constructing a ygm_ptr before using in an async manner.   For example, use
   * ygm_ptr::check(comm&);
   *
   * @param t
   */
  ygm_ptr(T *t) {
    idx = sptrs.size();
    sptrs.push_back(t);
  }

  ygm_ptr(const ygm::ygm_ptr<T> &t) { idx = t.idx; }

  T *get_raw_pointer() { return operator->(); }

  uint32_t index() const { return idx; }

  void check(comm &c) const { YGM_ASSERT_RELEASE(idx == ::ygm::min(idx, c)); }

  template <class Archive>
  void serialize(Archive &archive) {
    archive(idx);
  }

 private:
  uint32_t                idx;
  static std::vector<T *> sptrs;
};

template <typename T>
std::vector<T *> ygm_ptr<T>::sptrs;

}  // end namespace ygm
