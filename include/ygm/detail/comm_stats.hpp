// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <mpi.h>

namespace ygm {
class comm;

namespace detail {
class comm_stats {
 public:
  friend class ygm::comm;

  class timer {
   public:
    timer(double& _timer) : m_timer(_timer), m_start_time(MPI_Wtime()) {}

    ~timer() { m_timer += (MPI_Wtime() - m_start_time); }

   private:
    double& m_timer;
    double  m_start_time;
  };

  comm_stats() : m_time_start(MPI_Wtime()) {}

  void reset() {
    m_async_count                = 0;
    m_rpc_count                  = 0;
    m_route_count                = 0;
    m_isend_count                = 0;
    m_isend_bytes                = 0;
    m_isend_test_count           = 0;
    m_irecv_count                = 0;
    m_irecv_bytes                = 0;
    m_irecv_test_count           = 0;
    m_waitsome_isend_irecv_time  = 0.0f;
    m_waitsome_isend_irecv_count = 0.0f;
    m_iallreduce_count           = 0;
    m_waitsome_iallreduce_time   = 0.0f;
    m_waitsome_iallreduce_count  = 0;
    m_time_start                 = MPI_Wtime();
  }

  size_t get_async_count() const { return m_async_count; }
  size_t get_rpc_count() const { return m_rpc_count; }
  size_t get_route_count() const { return m_route_count; }

  size_t get_isend_count() const { return m_isend_count; }
  size_t get_isend_bytes() const { return m_isend_bytes; }
  size_t get_isend_test_count() const { return m_isend_test_count; }

  size_t get_irecv_count() const { return m_irecv_count; }
  size_t get_irecv_bytes() const { return m_irecv_bytes; }
  size_t get_irecv_test_count() const { return m_irecv_test_count; }

  double get_waitsome_isend_irecv_time() const {
    return m_waitsome_isend_irecv_time;
  }
  size_t get_waitsome_isend_irecv_count() const {
    return m_waitsome_isend_irecv_count;
  }

  size_t get_iallreduce_count() const { return m_iallreduce_count; }
  double get_waitsome_iallreduce_time() const {
    return m_waitsome_iallreduce_time;
  }
  size_t get_waitsome_iallreduce_count() const {
    return m_waitsome_iallreduce_count;
  }

  double get_elapsed_time() const { return MPI_Wtime() - m_time_start; }

 private:
  void isend([[maybe_unused]] int dest, size_t bytes) {
    m_isend_count += 1;
    m_isend_bytes += bytes;
  }

  void irecv([[maybe_unused]] int source, size_t bytes) {
    m_irecv_count += 1;
    m_irecv_bytes += bytes;
  }

  void async([[maybe_unused]] int dest) { m_async_count += 1; }

  void rpc_execute() { m_rpc_count += 1; }

  void routing() { m_route_count += 1; }

  void isend_test() { m_isend_test_count += 1; }

  void irecv_test() { m_irecv_test_count += 1; }

  void iallreduce() { m_iallreduce_count += 1; }

  timer waitsome_isend_irecv() {
    m_waitsome_isend_irecv_count += 1;
    return timer(m_waitsome_isend_irecv_time);
  }

  timer waitsome_iallreduce() {
    m_waitsome_iallreduce_count += 1;
    return timer(m_waitsome_iallreduce_time);
  }

  size_t m_async_count = 0;
  size_t m_rpc_count   = 0;
  size_t m_route_count = 0;

  size_t m_isend_count      = 0;
  size_t m_isend_bytes      = 0;
  size_t m_isend_test_count = 0;

  size_t m_irecv_count      = 0;
  size_t m_irecv_bytes      = 0;
  size_t m_irecv_test_count = 0;

  double m_waitsome_isend_irecv_time  = 0.0f;
  size_t m_waitsome_isend_irecv_count = 0.0f;

  size_t m_iallreduce_count          = 0;
  double m_waitsome_iallreduce_time  = 0.0f;
  size_t m_waitsome_iallreduce_count = 0;

  double m_time_start = 0.0;
};
}  // namespace detail
}  // namespace ygm
