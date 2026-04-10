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

namespace ygm {
class comm;

namespace detail {

struct sigaction old_SIGINT_action;
struct sigaction old_SIGTERM_action;
struct sigaction old_SIGSEGV_action;
struct sigaction old_SIGHUP_action;

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
    instance = this;
    reset();
  }

  ~comm_stats() {
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

  /**************************************
   * SIGNAL HANDLERS FOR CLEANUP
   **************************************/

  static void chained_SIGINT_handler(int sig) {
    const char* msg = "Chained handler caught SIGINT. Initiating shm Unlink.\n";
    write(STDOUT_FILENO, msg, strlen(msg)); // Async-signal-safe methods

    if (instance) {
      instance->unlinking_handler(sig);
    }

    // Call the previous handler if exists unless SIG_DFL or SIG_IGN
    if (old_SIGINT_action.sa_handler != SIG_DFL && old_SIGINT_action.sa_handler != SIG_IGN) {
      old_SIGINT_action.sa_handler(sig);
    }
  }

  static void chained_SIGTERM_handler(int sig) {
    const char* msg = "Chained handler caught SIGTERM. Initiating Shm Unlink.\n";
    write(STDOUT_FILENO, msg, strlen(msg)); // Async-signal-safe methods

    if (instance) {
      instance->unlinking_handler(sig);
    }

    // Call the previous handler if exists unless SIG_DFL or SIG_IGN
    if (old_SIGTERM_action.sa_handler != SIG_DFL && old_SIGTERM_action.sa_handler != SIG_IGN) {
      old_SIGTERM_action.sa_handler(sig);
    }
  }

  static void chained_SIGSEGV_handler(int sig) {
    const char* msg = "Chained handler caught SIGSEGV. Initiating Shm Unlink.\n";
    write(STDOUT_FILENO, msg, strlen(msg)); // Async-signal-safe methods

    if (instance) {
      instance->unlinking_handler(sig);
    }

    // Call the previous handler if exists unless SIG_DFL or SIG_IGN
    if (old_SIGSEGV_action.sa_handler != SIG_DFL && old_SIGSEGV_action.sa_handler != SIG_IGN) {
      old_SIGSEGV_action.sa_handler(sig);
    }
  }

  static void chained_SIGHUP_handler(int sig) {
    const char* msg = "Chained handler caught SIGHUP. Initiating Shm Unlink.\n";
    write(STDOUT_FILENO, msg, strlen(msg)); // Async-signal-safe methods

    if (instance) {
      instance->unlinking_handler(sig);
    }

    // Call the previous handler if exists unless SIG_DFL or SIG_IGN
    if (old_SIGHUP_action.sa_handler != SIG_DFL && old_SIGHUP_action.sa_handler != SIG_IGN) {
      old_SIGHUP_action.sa_handler(sig);
    }
  }



  void unlinking_handler(int) {
    if (stats != &m_local_stats){
      if (m_fd != -1) {
        close(m_fd); // async signal safe
        shm_unlink(m_stats_path.c_str()); // Not async signal safe but necessary
      }
      if (m_owns_manifest) {
        shm_unlink(m_manifest_path.c_str()); // Not async signal safe but necessary
      }
    }
  }

  /**************************************
   * END SIGNAL HANDLERS
   **************************************/

  void setup_shm(int rank, int comm_size, std::string uuid) {
    // register unexpected exit cleanups

    // create structures to hold SIGINT cleanup routines
    struct sigaction new_SIGINT_action;
    memset(&new_SIGINT_action, 0, sizeof(new_SIGINT_action));
    new_SIGINT_action.sa_handler = chained_SIGINT_handler;

    // register & recapture old routines
    if (sigaction(SIGINT, &new_SIGINT_action, &old_SIGINT_action) < 0) {
      perror("sigaction failure.");
      std::cout << "sigaction failure." << std::endl;
      return;
    }

    // create structures to hold SIGTERM cleanup routines
    struct sigaction new_SIGTERM_action;
    memset(&new_SIGTERM_action, 0, sizeof(new_SIGTERM_action));
    new_SIGTERM_action.sa_handler = chained_SIGTERM_handler;

    // register & recapture
    if (sigaction(SIGTERM, &new_SIGTERM_action, &old_SIGTERM_action) < 0) {
      perror("sigaction failure on SIGTERM.");
      std::cout << "sigaction failure on SIGTERM." << std::endl;
      return;
    }

    // create structures to hold SIGSEGV cleanup routines
    struct sigaction new_SIGSEGV_action;
    memset(&new_SIGSEGV_action, 0, sizeof(new_SIGSEGV_action));
    new_SIGSEGV_action.sa_handler = chained_SIGSEGV_handler;

    // register & recapture
    if (sigaction(SIGSEGV, &new_SIGSEGV_action, &old_SIGSEGV_action) < 0) {
      perror("sigaction failure on SIGSEGV.");
      std::cout << "sigaction failure on SIGSEGV." << std::endl;
      return;
    }


    // set UUID for output
    m_uuid = uuid;

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
  }

  void write_manifest(const std::vector<int>& local_ranks) {
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

  // Shm lifecycle components (only meaningful when monitor enabled)
  int         m_fd             = -1;
  std::string m_stats_path;
  std::string m_manifest_path;
  bool        m_owns_manifest  = false;

  std::string m_uuid; // set during setup, empty when shm disabled

  // Captured at construction for timing
  double m_time_start;

  // instance storage for use with signal handling
  static comm_stats* instance;
};

comm_stats* comm_stats::instance = nullptr;

}  // namespace detail
}  // namespace ygm
