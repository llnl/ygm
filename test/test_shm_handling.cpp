// Copyright 2019-2026 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG

#include <ygm/comm.hpp>
#include <ygm/detail/stats_shm_signal.hpp>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

int main(int argc, char** argv) {

  /** Infrastructure verification scope */
  {
  {
    // Prior-handler preservation. Install a sentinel on SIGTERM *before*
    // any register_path call, trigger the lazy handler install via one
    // register_path, and verify shm::old_actions captured the sentinel.
    // Must run first -- ensure_handlers_registered is one-shot, so only
    // the first register_path call populates old_actions.
    namespace shm = ygm::detail::shm;

    void (*sentinel)(int) = +[](int){};
    struct sigaction prior;
    std::memset(&prior, 0, sizeof(prior));
    prior.sa_handler = sentinel;
    sigaction(SIGTERM, &prior, nullptr);

    assert(shm::register_path("/ygm_chain_probe"));

    int sigterm_idx = -1;
    for (size_t i = 0; i < shm::num_tracked_signals; ++i) {
      if (shm::tracked_signals[i] == SIGTERM) {
        sigterm_idx = static_cast<int>(i);
        break;
      }
    }
    assert(sigterm_idx != -1);
    assert(shm::old_actions[sigterm_idx].sa_handler == sentinel);

    shm::unregister_path("/ygm_chain_probe");
    assert(shm::num_active_paths == 0);
  }

  {
    // Handler-install readback. After the first register_path has run,
    // SIGTERM's current disposition should be our handler registered
    // with SA_SIGINFO. Catches regressions that skip the install path
    // or drop SA_SIGINFO (which would silently break MPI interop).
    namespace shm = ygm::detail::shm;

    struct sigaction current;
    assert(sigaction(SIGTERM, nullptr, &current) == 0);
    assert(current.sa_sigaction == shm::chained_unlink_handler);
    assert((current.sa_flags & SA_SIGINFO) != 0);
  }

  {
    // Direct register/unregister tests. Run before any ygm::comm is
    // constructed so the tracking array is empty and no live entries get
    // disturbed.
    namespace shm = ygm::detail::shm;
    assert(shm::num_active_paths == 0);

    // Fill to capacity, verify the next registration refuses.
    for (int i = 0; i < shm::MAX_SHM_PATHS; ++i) {
      char buf[32];
      std::snprintf(buf, sizeof(buf), "/ygm_test_%d", i);
      assert(shm::register_path(buf));
    }
    assert(shm::num_active_paths == shm::MAX_SHM_PATHS);
    assert(!shm::register_path("/ygm_test_overflow"));

    // Drain and confirm the bookkeeping matches.
    for (int i = 0; i < shm::MAX_SHM_PATHS; ++i) {
      char buf[32];
      std::snprintf(buf, sizeof(buf), "/ygm_test_%d", i);
      shm::unregister_path(buf);
    }
    assert(shm::num_active_paths == 0);

    // Unknown-path unregister must be a silent no-op.
    shm::unregister_path("/ygm_never_registered");
    assert(shm::num_active_paths == 0);
  }

  /* { 
    // Scoped-comm lifecycle check. Construct a comm in a nested scope,
    // capture its staged shm path, and after the comm destructs verify
    // both the tracking array and the /dev/shm entry are gone. Catches
    // regressions where close_shm stops firing on normal exit.
    //
    // Per rank: each rank's path is unique (uuid is shared, rank suffix
    // differs), so shm_open on that rank's captured path is the right
    // probe locally.
    char captured_path[ygm::detail::shm::kShmPathMaxLen] = {0};
    {
      ygm::comm scoped(&argc, &argv);
      assert(ygm::detail::shm::num_active_paths == 1);
      std::strncpy(captured_path,
                  ygm::detail::shm::active_shm_paths[0],
                  sizeof(captured_path));
      captured_path[sizeof(captured_path) - 1] = '\0';
    }
    assert(ygm::detail::shm::num_active_paths == 0);
    int fd = shm_open(captured_path, O_RDONLY, 0);
    assert(fd == -1 && errno == ENOENT);
  } */
  }

  // Make several comms and ensure coverage for all
  ygm::comm world(&argc, &argv);
  ygm::comm world1(&argc, &argv);
  // ygm::comm world2(&argc, &argv);

  // Pre-signal sanity: three live comms should have staged three paths.
  // If this fires, close_shm already ran on a live comm, or open_shm
  // silently refused -- both would make the downstream crash cases lie.
  // assert(ygm::detail::shm::num_active_paths == 3);
  
  int num_rounds = 120;
  for (int i = 0; i < num_rounds; ++i) {

    /**
     *  Unless a command line argument is passed, demo will behave as normal 
     *  and wait for an outside signal if handling testing desired. Otherwise
     *  exits as normal. 
     */

    // Send some async messages to generate stats
    for (int dest = 0; dest < world.size(); ++dest) {
      world.async(dest, [](int){}, world.rank());
      std::this_thread::sleep_for(std::chrono::milliseconds(world.rank() * 250)); 
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));

    world.barrier();
    world.cout0() << "Barrier " << i + 1 << "/" << num_rounds << "\n" << std::flush;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Specific internal failure scenario testing.

    if (argc > 1 && strcmp(argv[1], "all-trip-oom") == 0){

      world.cout0() << "Filling vector until oom" << std::endl;
      int j = 1;

      std::vector<int> overflow_vec;
  
      // flood vector until a process runs out of available mem
      while (true) {
        overflow_vec.push_back(j++);
      }

      /**
       * Local MPIRUN:
       * * first offending process is sent SIGKILL, 
       *    mpirun? observes and sends SIGTERM to remaining processes
       * 
       * Cluster SBATCH:
       * 
       * Cluster SRUN:
       * 
       * Cluster SALLOC:
       */
    }

    if (argc > 1 && strcmp(argv[1], "zero-trip-oom") == 0){
      
      world.cout0() << "Filling vector until oom" << std::endl;
      int j = 1;

      std::vector<int> overflow_vec;
  
      if (world.rank0()) {
        // flood vector until a process runs out of available mem
        while (true) {
          overflow_vec.push_back(j++);
        }
      }
      else {
        while (true) {
          j++; // spin on j waiting for overflow to trip.
        }
      }

      /**
       * Local MPIRUN:
       * 
       * Cluster SBATCH:
       * 
       * Cluster SRUN:
       * 
       * Cluster SALLOC:
       */
    }

    if (argc > 1 && strcmp(argv[1], "all-trip-seg") == 0){
      /**
       * Local MPIRUN:
       * * race winner segfaults and remaining processes are
       *    given a copy of the signal and terminate gracefully.
       * 
       * Cluster SBATCH:
       * 
       * Cluster SRUN:
       * 
       */

      world.cout0() << "Segfault requested." << std::endl;
      int* ptr = nullptr;
      *ptr = 42;
    }

    if (argc > 1 && strcmp(argv[1], "zero-trip-seg") == 0){
      /**
       * Local MPIRUN:
       * 
       * Cluster SBATCH:
       * 
       * Cluster SRUN:
       * 
       */

      world.cout0() << "Segfault requested." << std::endl;
      if (world.rank0()) {
        int* ptr = nullptr;
        *ptr = 42;
      }
    }
  }

  return 0;
}
