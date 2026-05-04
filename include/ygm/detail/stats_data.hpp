// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

namespace ygm::detail {

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
  uint32_t m_local_size;
  uint32_t m_comm_size;

  // Communication counters
  uint64_t m_async_count;
  uint64_t m_barrier_count;
  // TODO: add m_async_barrier_count once async-barrier instrumentation lands.
  uint64_t m_rpc_count;
  uint64_t m_route_count;

  uint64_t m_large_buffer_send_count;
  uint64_t m_large_buffer_recv_count;

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
  int64_t m_last_barrier_utc;

  // TODO: add double m_last_barrier_duration once per-barrier timing is wired in.

  // TODO: u64 buffer-utilization counters (future). Candidates:
  //   m_pending_isend_bytes, m_send_local_buffer_bytes,
  //   m_send_remote_buffer_bytes, m_send_queue_depth, m_recv_queue_depth.
};

}  // namespace ygm::detail
