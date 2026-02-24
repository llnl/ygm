// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

namespace ygm::detail {

/**
 * @brief Header for the per-node manifest POSIX shm segment.
 *
 * @details Written by the lowest-local-rank on each node so ygm-top can
 * discover which global ranks are node-local. Layout in shm:
 *   [manifest_header][manifest_entry][manifest_entry]...
 */
struct manifest_header {
  uint64_t local_rank_count;
};

/**
 * @brief One entry per local rank in the manifest segment.
 */
struct manifest_entry {
  uint64_t global_rank;
  // uint64_t pid;  // future: add per-rank metadata here
};

/**
 * @brief Shared memory layout for per-rank YGM performance counters.
 *
 * @details Uses fixed-width types only (uint64_t, uint32_t, double) to avoid
 * ABI issues between the YGM program and ygm-top reader. When stat sharing is
 * disabled, an instance of this struct lives as a local member of comm_stats.
 * When enabled, the struct lives in an mmap'd POSIX shm region.
 */
struct stats_data {
  // Identity
  uint32_t m_rank;
  uint32_t m_comm_size;

  // Communication counters
  uint64_t m_async_count;
  uint64_t m_barrier_count;
  // uint64_t m_async_barrier_count;
  uint64_t m_rpc_count;
  uint64_t m_route_count;

  uint64_t m_isend_count;
  uint64_t m_isend_bytes;
  uint64_t m_isend_test_count;

  uint64_t m_irecv_count;
  uint64_t m_irecv_bytes;
  uint64_t m_irecv_test_count;

  uint64_t m_iallreduce_count;
  uint64_t m_waitsome_isend_irecv_count;
  uint64_t m_waitsome_iallreduce_count;

  // Timing
  double m_waitsome_isend_irecv_time;
  double m_waitsome_iallreduce_time;
  double m_time_start;

  // double m_last_barrier_duration;

  /*
  Potential features to be implemented at a later date

  // Buffer Utilization
  uint64_t m_pending_isend_bytes;
  uint64_t m_send_local_buffer_bytes;
  uint64_t m_send_remote_buffer_bytes;
  uint64_t m_send_queue_depth;
  uint64_t m_recv_queue_depth;
  */
};

}  // namespace ygm::detail
