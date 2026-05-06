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
  /*
  {
    // Prior-handler preservation. Install a sentinel on SIGTERM *before*
    // any register_path call, and verify shm::old_actions captured the sentinel.
    namespace shm = ygm::detail::shm;

    // Without chaining downstream handlers, this will break the MPI-level SIGTERM
    // cleanup. 
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
  */

  {
    // Handler-install readback. After the first register_path has run,
    // SIGTERM's current disposition should be ygm handler registered
    // with SA_SIGINFO. Catches regressions that skip the install path
    // or drop SA_SIGINFO, silently breaking MPI interop.
    namespace shm = ygm::detail::shm;
    ygm::detail::shm::ensure_handlers_registered();

    struct sigaction current;
    assert(sigaction(SIGTERM, nullptr, &current) == 0);
    assert(current.sa_sigaction == shm::chained_unlink_handler);
    assert((current.sa_flags & SA_SIGINFO) != 0);
  }
  }

  // Make several comms and ensure coverage for all
  ygm::comm world(&argc, &argv);
  ygm::comm world1(world.get_mpi_comm());
  ygm::comm world2(world.get_mpi_comm());

  // Pre-signal sanity check.
  assert(ygm::detail::live_comm_uuids.size() == 3);

  // Scoped-comm lifecycle check.
  {
    ygm::comm scoped(world.get_mpi_comm());
    assert(ygm::detail::live_comm_uuids.size() == 4);
  }

  // Check that shm path is no longer open.
  assert(ygm::detail::live_comm_uuids.size() == 3);
  // int fd = shm_open(captured_path, O_RDONLY, 0);
  // assert(fd == -1 && errno == ENOENT);

  // Demo operation or requested failure mode
  int num_rounds = 40;
  for (int i = 0; i < num_rounds; ++i) {

    /**
     *  Unless a command line argument is passed, demo will behave as normal 
     *  and wait for an outside signal if handling testing desired. Otherwise
     *  demos function and exits as normal. 
     */

    // Send some async messages to generate stats
    for (int dest = 0; dest < world.size(); ++dest) {
      world.async(dest, [](int){}, world.rank());
      std::this_thread::sleep_for(std::chrono::milliseconds(world.rank() * 1000 / world.size())); 
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));

    world.barrier();
    world.cout0() << "Barrier " << i + 1 << "/" << num_rounds << "\n" << std::flush;
    std::this_thread::sleep_for(std::chrono::seconds(2));


    // Specific internal failure scenario testing.

    if (argc > 1 && strcmp(argv[1], "all-trip-oom") == 0){
      world.cout0() << "Filling vector until out of memory in race conditions" << std::endl;

      int j = 1;
      std::vector<int> overflow_vec;
  
      // flood vectors until a process runs out of available mem
      while (true) {
        overflow_vec.push_back(j++);
      }
    }

    if (argc > 1 && strcmp(argv[1], "zero-trip-oom") == 0){
      world.cout0() << "Filling vector until out of memory" << std::endl;

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
          j++; // if not rank0, spin on j waiting for overflow to trip.
        }
      }
    }

    if (argc > 1 && strcmp(argv[1], "all-trip-seg") == 0){
      world.cout0() << "Race condition segfault requested." << std::endl;
      int* ptr = nullptr;
      *ptr = 0;
    }

    if (argc > 1 && strcmp(argv[1], "zero-trip-seg") == 0){
      world.cout0() << "Rank 0 segfault requested." << std::endl;
      if (world.rank0()) {
        int* ptr = nullptr;
        *ptr = 0;
      }

      world.barrier();
    }
  }

  return 0;
}
