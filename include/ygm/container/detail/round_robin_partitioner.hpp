// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

namespace ygm::container::detail {

/**
 * @brief Assigns items to ranks according to a cyclic distribution
 *
 */
struct round_robin_partitioner {
  round_robin_partitioner(const ygm::comm &comm)
      : m_next(comm.rank()), m_comm_size(comm.size()) {}

  /**
   * @brief Assigns item to next rank for storage
   *
   * @return Next rank in cycle
   */
  template <typename Item>
  int owner(const Item &) {
    if (++m_next >= m_comm_size) {
      m_next = 0;
    }
    return m_next;
  }
  int m_next;
  int m_comm_size;
};

}  // namespace ygm::container::detail
