// Copyright 2019-2026 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <mpi.h>

#include <iostream>
#include <string>
#include <ctime>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <ygm/detail/stats_data.hpp>
#include <ygm/detail/stats_shm_signal.hpp>
#include <ygm/detail/ygm_uuids.hpp>

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

  comm_stats()
      : stats(&m_local_stats), m_time_start(MPI_Wtime()) {
    reset();
  }

  ~comm_stats() {
    if (stats != &m_local_stats) close_comm_stats_shm();
  }

  void reset() {
    stats->m_async_count                = 0;
    stats->m_barrier_count              = 0;
    stats->m_rpc_count                  = 0;
    stats->m_route_count                = 0;
    stats->m_large_buffer_send_count    = 0;
    stats->m_large_buffer_recv_count    = 0;
    stats->m_isend_count                = 0;
    stats->m_isend_bytes                = 0;
    stats->m_isend_test_count           = 0;
    stats->m_irecv_count                = 0;
    stats->m_irecv_bytes                = 0;
    stats->m_irecv_test_count           = 0;
    stats->m_iallreduce_count           = 0;
    stats->m_waitsome_isend_irecv_count = 0;
    stats->m_waitsome_iallreduce_count  = 0;
    stats->m_waitsome_isend_irecv_time  = 0.0;
    stats->m_waitsome_iallreduce_time   = 0.0;
    stats->m_time_start                 = MPI_Wtime();
  }

  size_t get_async_count() const { return stats->m_async_count; }
  size_t get_barrier_count() const { return stats->m_barrier_count; }
  size_t get_rpc_count() const { return stats->m_rpc_count; }
  size_t get_route_count() const { return stats->m_route_count; }

  size_t get_isend_count() const { return stats->m_isend_count; }
  size_t get_isend_bytes() const { return stats->m_isend_bytes; }
  size_t get_isend_test_count() const { return stats->m_isend_test_count; }

  size_t get_irecv_count() const { return stats->m_irecv_count; }
  size_t get_irecv_bytes() const { return stats->m_irecv_bytes; }
  size_t get_irecv_test_count() const { return stats->m_irecv_test_count; }

  size_t get_large_buffer_send_count() const {
    return stats->m_large_buffer_send_count;
  }
  size_t get_large_buffer_recv_count() const {
    return stats->m_large_buffer_recv_count;
  }

  double get_waitsome_isend_irecv_time() const {
    return stats->m_waitsome_isend_irecv_time;
  }
  size_t get_waitsome_isend_irecv_count() const {
    return stats->m_waitsome_isend_irecv_count;
  }

  size_t get_iallreduce_count() const { return stats->m_iallreduce_count; }
  double get_waitsome_iallreduce_time() const {
    return stats->m_waitsome_iallreduce_time;
  }
  size_t get_waitsome_iallreduce_count() const {
    return stats->m_waitsome_iallreduce_count;
  }

  double get_elapsed_time() const { return MPI_Wtime() - stats->m_time_start; }

 private:
  // open_shm / close_shm form the shm lifecycle pair. open_shm registers
  // for signal cleanup, creates the segment, and swings `stats` ptr to it
  // or rolls back fully on any failure. close_shm is called from
  // ~comm_stats only if open_shm previously succeeded.

  void close_comm_stats_shm() {
    ygm::detail::live_comm_uuids.erase(m_stats_path.substr(5));
    shm_unlink(m_stats_path.c_str());
    munmap(stats, sizeof(stats_data));
  }

  void open_comm_stats_shm(int rank, int comm_size, int local_size, std::string path_id) {

    shm::ensure_handlers_registered();
    m_stats_path = shm::shm_prefix + path_id;

    // Open shm segment and cleanup if failure
    int fd = shm_open(m_stats_path.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0600);
    if (fd == -1) { 
      std::cerr << "ygm::comm_stats: shm_open failed for " << m_stats_path
                << ": " << strerror(errno) << std::endl;
      return;
    }

    // Size shm segment, cleanup if failed
    if (ftruncate(fd, sizeof(stats_data)) == -1) {
      std::cerr << "ygm::comm_stats: ftruncate failed for " << m_stats_path
                << ": " << strerror(errno) << std::endl;
      close(fd);
      shm_unlink(m_stats_path.c_str());
      return;
    }

    // mmap, clean if fail
    void* region = mmap(NULL, sizeof(stats_data), PROT_READ | PROT_WRITE,
                        MAP_SHARED, fd, 0);
    if (region == MAP_FAILED) {
      std::cerr << "ygm::comm_stats: mmap failed for " << m_stats_path << ": "
                << strerror(errno) << std::endl;
      close(fd);
      shm_unlink(m_stats_path.c_str());
      return;
    }

    close(fd); // after mmapped, file descriptor isn't needed to access region.

    // Swing pointer to shared memory region
    stats = static_cast<stats_data*>(region);

    // Initialize the shm region
    reset();
    stats->m_rank      = static_cast<uint32_t>(rank);
    stats->m_comm_size = static_cast<uint32_t>(comm_size);
    stats->m_local_size = static_cast<uint32_t>(local_size);
    stats->m_time_start = m_time_start;
  }

  void isend([[maybe_unused]] int dest, size_t bytes) {
    stats->m_isend_count += 1;
    stats->m_isend_bytes += bytes;
  }

  void irecv([[maybe_unused]] int source, size_t bytes) {
    stats->m_irecv_count += 1;
    stats->m_irecv_bytes += bytes;
  }

  void large_buffer_send([[maybe_unused]] int dest) {
    stats->m_large_buffer_send_count += 1;
  }

  void large_buffer_recv([[maybe_unused]] int source) {
    stats->m_large_buffer_recv_count += 1;
  }

  void async([[maybe_unused]] int dest) { stats->m_async_count += 1; }

  void barrier() {
    stats->m_barrier_count += 1;
    stats->m_last_barrier_utc = static_cast<int64_t>(std::time(nullptr));
  }

  void rpc_execute() { stats->m_rpc_count += 1; }

  void routing() { stats->m_route_count += 1; }

  void isend_test() { stats->m_isend_test_count += 1; }

  void irecv_test() { stats->m_irecv_test_count += 1; }

  void iallreduce() { stats->m_iallreduce_count += 1; }

  timer waitsome_isend_irecv() {
    stats->m_waitsome_isend_irecv_count += 1;
    return timer(stats->m_waitsome_isend_irecv_time);
  }

  timer waitsome_iallreduce() {
    stats->m_waitsome_iallreduce_count += 1;
    return timer(stats->m_waitsome_iallreduce_time);
  }

  // Backing storage when sharing via shm is disabled
  stats_data m_local_stats{};

  // Active storage pointer to shm or backing storage
  stats_data* stats;

  // Shm segment path, used for open/close under normal conditions
  std::string m_stats_path;

  // Captured at construction for timing
  double m_time_start;
};

}  // namespace detail
}  // namespace ygm
