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
#include <csignal>
#endif

#include <ygm/detail/stats_data.hpp>
#include <ygm/detail/ygm_uuids.hpp>

namespace ygm {
class comm;

namespace detail {

namespace shm {

/* +++ SHM COMPONENT NOTES +++
// ---------------------------------------------------------------------------
// Process-wide signal handling for shm cleanup on abnormal exit.
//
// This is process-global state (one signal disposition per process), not
// per-comm_stats state, so it lives at namespace scope rather than as class
// members. comm_stats::setup_shm() calls register_signal_handlers() once per
// process and sets process_mpi_rank before any signal can fire. All required 
// info for cleanup is set before segments are created.
// ---------------------------------------------------------------------------
*/

inline int process_mpi_rank = -1; 

// Dev note: Storage method may need to change if there is churn in communicators.
constexpr int MAX_SHM_UUIDS = 8;
inline int num_active_uuids = 0;
inline char active_shm_uuids[MAX_SHM_UUIDS][36] = {0};

inline bool process_signal_handlers_registered = false;

// Signals to intercept for shm cleanup on abnormal exit.
// No SIGKILL or SIGSTOP - cannot be caught
constexpr int tracked_signals[] = { 
  SIGHUP,  SIGINT,  SIGQUIT, SIGILL,  SIGTRAP, 
  SIGABRT, SIGBUS,  SIGFPE,  SIGSEGV, SIGPIPE, // SIGPIPE occasionally used by MPI on TCP. Disable if mpi handled
  SIGTERM, SIGSTKFLT, SIGXCPU, SIGXFSZ, SIGVTALRM,
  SIGPWR, SIGSYS
};

constexpr size_t num_tracked_signals =
    sizeof(tracked_signals) / sizeof(tracked_signals[0]);

// Saved previous handlers, parallel to tracked_signals[].
inline struct sigaction old_actions[num_tracked_signals];

inline void chained_ygm_unlink_handler(int sig) {
  // Diagnostic via async-signal-safe write()
  const char* usrmsg1 = "Caught signal ";
  char signum[2] = {static_cast<char>(sig / 10 % 10 + '0'),
                    static_cast<char>(sig % 10 + '0')};
  const char* usrmsg2 =
      " in chained handler. Initiating unlink for ygm shm segments.\n";

  std::ignore = write(STDOUT_FILENO, usrmsg1, 15);
  std::ignore = write(STDOUT_FILENO, signum, 2);
  std::ignore = write(STDOUT_FILENO, usrmsg2, 61);

  // Construct path string template in signal-safe way
  char current_shm_path[49];
  memcpy(&current_shm_path[0], "ygm_", 4);
  memcpy(&current_shm_path[40], "_rank", 5);
  current_shm_path[45] = static_cast<char>(process_mpi_rank / 100 % 10 + '0');
  current_shm_path[46] = static_cast<char>(process_mpi_rank / 10 % 10 + '0');
  current_shm_path[47] = static_cast<char>(process_mpi_rank % 10 + '0');
  current_shm_path[48] = '\0';

  // unlink with path template and uuid list
  for (int i = 0; i < num_active_uuids; i++){
    memcpy(&current_shm_path[4], shm::active_shm_uuids[i], 36);
    shm_unlink(current_shm_path);
  }

  // Call previous handler if was not default or ignore.
  for (size_t i = 0; i < num_tracked_signals; ++i) {
    if (tracked_signals[i] == sig
        && old_actions[i].sa_handler != SIG_DFL
        && old_actions[i].sa_handler != SIG_IGN) {
      old_actions[i].sa_handler(sig);
      break;
    }
  }

  // Restore default disposition and re-raise so process
  // terminates with correct signal / exit status.
  signal(sig, SIG_DFL);
  raise(sig);
}

inline void register_signal_handlers() {
  process_signal_handlers_registered = true;

  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = chained_ygm_unlink_handler;

  for (size_t i = 0; i < num_tracked_signals; ++i) {
    if (sigaction(tracked_signals[i], &sa, &old_actions[i]) < 0) {
      perror("ygm: sigaction registration failure.\n");
    }
  }
}

} // end namespace shm

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
    if (stats != &m_local_stats) {
      munmap(stats, sizeof(stats_data));
      shm_unlink(m_stats_path.c_str());
    }
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
  void setup_shm(int rank, int comm_size, int local_size, std::string uuid) {
    // Prep signal handling support for abnormal exit cleanup
    if (shm::num_active_uuids == shm::MAX_SHM_UUIDS) {
      std::cerr << "Could not allocate additional shm_stats due to uuid limit." << std::endl;
      std::cerr << "Increase max shm uuids in comm_stats.hpp." << std::endl;
      return;
    }
    
    shm::process_mpi_rank = rank;
    memcpy(shm::active_shm_uuids[shm::num_active_uuids], uuid.c_str(), 36);
    shm::num_active_uuids++;

    if (!shm::process_signal_handlers_registered) {
      shm::register_signal_handlers();
    }
    
    // Continue to shm region initialization
    m_uuid = uuid;
    m_stats_path = "/ygm_" + m_uuid + "_rank" + std::format("{:03d}", rank);

    // Create and size the shm object
    int fd = shm_open(m_stats_path.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0600);
    if (fd == -1) {
      std::cerr << "ygm::comm_stats: shm_open failed for " << m_stats_path
                << ": " << strerror(errno) << std::endl;
      return;
    }

    if (ftruncate(fd, sizeof(stats_data)) == -1) {
      std::cerr << "ygm::comm_stats: ftruncate failed for " << m_stats_path
                << ": " << strerror(errno) << std::endl;
      close(fd);
      shm_unlink(m_stats_path.c_str());
      return;
    }

    // Map into address space
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
    stats->m_local_ranks = static_cast<uint32_t>(local_size);
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

  // Backing storage when sharing via shm is disabled
  stats_data m_local_stats{};

  // Active storage pointer to shm or backing storage
  stats_data* stats;

  // Shm lifecycle components (empty if shm disabled)
  std::string m_stats_path;
  std::string m_uuid;

  // Captured at construction for timing
  double m_time_start;
};

}  // namespace detail
}  // namespace ygm
