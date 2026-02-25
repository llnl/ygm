// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <mpi.h>

#ifdef __linux__
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <ygm/detail/stats_data.hpp>

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
#ifdef __linux__
    if (stats != &m_local_stats) {
      munmap(stats, sizeof(stats_data));
      if (m_fd != -1) {
        close(m_fd);
        shm_unlink(m_stats_path.c_str());
      }
      if (m_owns_manifest) {
        shm_unlink(m_manifest_path.c_str());
      }
    }
#endif
  }

  void reset() {
    stats->m_async_count                = 0;
    stats->m_barrier_count              = 0;
    stats->m_rpc_count                  = 0;
    stats->m_route_count                = 0;
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
  void set_uuid(const std::string& uuid) { m_uuid = uuid; }

  void setup(int rank, int comm_size) {
#ifdef __linux__
    // Build shm path: /ygm_<UUID>_rank<RANK>
    m_stats_path = "/ygm_" + m_uuid + "_rank" + std::to_string(rank);

    // Create and size the shm object
    m_fd = shm_open(m_stats_path.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0600);
    if (m_fd == -1) {
      std::cerr << "ygm::comm_stats: shm_open failed for " << m_stats_path
                << ": " << strerror(errno) << std::endl;
      return;
    }

    if (ftruncate(m_fd, sizeof(stats_data)) == -1) {
      std::cerr << "ygm::comm_stats: ftruncate failed for " << m_stats_path
                << ": " << strerror(errno) << std::endl;
      close(m_fd);
      shm_unlink(m_stats_path.c_str());
      m_fd = -1;
      return;
    }

    // Map into address space
    void* region = mmap(NULL, sizeof(stats_data), PROT_READ | PROT_WRITE,
                        MAP_SHARED, m_fd, 0);
    if (region == MAP_FAILED) {
      std::cerr << "ygm::comm_stats: mmap failed for " << m_stats_path << ": "
                << strerror(errno) << std::endl;
      close(m_fd);
      shm_unlink(m_stats_path.c_str());
      m_fd = -1;
      return;
    }

    // Swing the pointer — this is the only "switch" in the entire design
    stats = static_cast<stats_data*>(region);

    // Initialize the shm region
    reset();
    stats->m_rank      = static_cast<uint32_t>(rank);
    stats->m_comm_size = static_cast<uint32_t>(comm_size);
    stats->m_time_start = m_time_start;
#else
    (void)rank;
    (void)comm_size;
#endif
  }

  void write_manifest(const std::vector<int>& local_ranks) {
#ifdef __linux__
    m_manifest_path  = "/ygm_" + m_uuid + "_manifest";
    m_owns_manifest  = true;

    size_t manifest_size = local_ranks.size() * sizeof(manifest_entry);

    int mfd = shm_open(m_manifest_path.c_str(), O_CREAT | O_TRUNC | O_RDWR,
                        0600);
    if (mfd == -1) {
      std::cerr << "ygm::comm_stats: shm_open failed for manifest: "
                << strerror(errno) << std::endl;
      m_owns_manifest = false;
      return;
    }

    if (ftruncate(mfd, static_cast<off_t>(manifest_size)) == -1) {
      std::cerr << "ygm::comm_stats: ftruncate failed for manifest: "
                << strerror(errno) << std::endl;
      close(mfd);
      shm_unlink(m_manifest_path.c_str());
      m_owns_manifest = false;
      return;
    }

    auto* region = static_cast<manifest_entry*>(
        mmap(NULL, manifest_size, PROT_READ | PROT_WRITE, MAP_SHARED, mfd, 0));
    if (region == MAP_FAILED) {
      std::cerr << "ygm::comm_stats: mmap failed for manifest: "
                << strerror(errno) << std::endl;
      close(mfd);
      shm_unlink(m_manifest_path.c_str());
      m_owns_manifest = false;
      return;
    }

    for (size_t i = 0; i < local_ranks.size(); ++i) {
      region[i].global_rank = local_ranks[i];
    }

    munmap(region, manifest_size);
    close(mfd);
#else
    (void)local_ranks;
#endif
  }

  void isend([[maybe_unused]] int dest, size_t bytes) {
    stats->m_isend_count += 1;
    stats->m_isend_bytes += bytes;
  }

  void irecv([[maybe_unused]] int source, size_t bytes) {
    stats->m_irecv_count += 1;
    stats->m_irecv_bytes += bytes;
  }

  void async([[maybe_unused]] int dest) { stats->m_async_count += 1; }

  void barrier() { stats->m_barrier_count += 1; }

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

  // Backing storage — used when monitor is disabled
  stats_data m_local_stats{};

  // Active storage pointer — always valid, never null
  stats_data* stats;

  // Shm lifecycle (only meaningful when monitor enabled)
  int         m_fd             = -1;
  std::string m_stats_path;
  std::string m_manifest_path;
  bool        m_owns_manifest  = false;

  // UUID for job isolation (set during setup, empty when disabled)
  std::string m_uuid;

  // Captured at construction for timing
  double m_time_start;
};
}  // namespace detail
}  // namespace ygm
