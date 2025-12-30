// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once
#include <bit>
#include <functional>

#include <ygm/comm.hpp>

namespace ygm::container::detail {

/**
 * @brief Partitioner for containers of fixed size that assigns contiguous
 * blocks of indices to a rank
 */
template <typename Index>
struct block_partitioner {
  using index_type = Index;

  /**
   * @brief block_partitioner constructor
   *
   * @param comm Underlying communicator being used by the container
   * @param partitioned_size Global size of the container being partitioned
   * @details Sets up block sizes to be used by the block_partitioner
   */
  block_partitioner(ygm::comm &comm, index_type partitioned_size)
      : m_comm_size(comm.size()),
        m_comm_rank(comm.rank()),
        m_partitioned_size(partitioned_size) {
    m_small_block_size = partitioned_size / m_comm_size;
    m_large_block_size =
        m_small_block_size + ((partitioned_size % m_comm_size) > 0);

    if (index_type(m_comm_rank) < (partitioned_size % m_comm_size)) {
      m_local_start_index = m_comm_rank * m_large_block_size;
    } else {
      m_local_start_index =
          (partitioned_size % m_comm_size) * m_large_block_size +
          (m_comm_rank - (partitioned_size % m_comm_size)) * m_small_block_size;
    }

    m_local_size = m_small_block_size + (index_type(m_comm_rank) <
                                         (m_partitioned_size % m_comm_size));

    if (index_type(m_comm_rank) < (m_partitioned_size % m_comm_size)) {
      m_local_start_index = m_comm_rank * m_large_block_size;
    } else {
      m_local_start_index =
          (m_partitioned_size % m_comm_size) * m_large_block_size +
          (m_comm_rank - (m_partitioned_size % m_comm_size)) *
              m_small_block_size;
    }
  }

  /**
   * @brief Calculate the owner of a given index
   *
   * @param index Index into container
   * @return Rank responsible for storing the associated item
   */
  int owner(const index_type &index) const {
    int to_return;
    // Owner depends on whether index is before switching to small blocks
    if (index < (m_partitioned_size % m_comm_size) * m_large_block_size) {
      YGM_ASSERT_RELEASE(m_large_block_size > 0);
      to_return = index / m_large_block_size;
    } else {
      if (m_small_block_size == 0) {
        std::cout << m_small_block_size << "\t" << m_large_block_size << "\t"
                  << m_partitioned_size << "\t" << m_comm_size << "\t" << index
                  << std::endl;
      }
      YGM_ASSERT_RELEASE(m_small_block_size > 0);
      to_return =
          (m_partitioned_size % m_comm_size) +
          (index - (m_partitioned_size % m_comm_size) * m_large_block_size) /
              m_small_block_size;
    }
    YGM_ASSERT_RELEASE((to_return >= 0) && (to_return < m_comm_size));

    return to_return;
  }

  /**
   * @brief Convert a global index into container into the index used for local
   * storage
   *
   * @param global_index Index into global container
   * @return Index into local storage
   */
  index_type local_index(const index_type &global_index) const {
    index_type to_return = global_index - m_local_start_index;
    YGM_ASSERT_RELEASE((to_return >= 0) && (to_return < m_local_size));
    return to_return;
  }

  /**
   * @brief Converts a local index for an item stored on this rank into its
   * global index
   *
   * @param local_index Index into locally-held items
   * @return Global index associated to local item index
   */
  index_type global_index(const index_type &local_index) const {
    index_type to_return = m_local_start_index + local_index;
    YGM_ASSERT_RELEASE(to_return < m_partitioned_size);
    return to_return;
  }

  /**
   * @brief Number of items stored locally
   *
   * @return Number of items stored on this rank
   */
  index_type local_size() const { return m_local_size; }

  /**
   * @brief Global index of first local item
   *
   * @return Beginning of global index space assigned to current rank
   */
  index_type local_start() const { return m_local_start_index; }

 private:
  int        m_comm_size;
  int        m_comm_rank;
  index_type m_partitioned_size;
  index_type m_small_block_size;
  index_type m_large_block_size;
  index_type m_local_size;
  index_type m_local_start_index;
};

}  // namespace ygm::container::detail
