// Copyright 2019-2026 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG

#include <ygm/comm.hpp>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

// Simple smoke test for the YGM monitor — runs barriers in a loop with sleeps
// so ygm-top has time to attach and display data.

int main(int argc, char** argv) {
  ygm::comm world(&argc, &argv);

  for (int i = 0; i < 10; ++i) {
    // Send some async messages to generate stats
    for (int dest = 0; dest < world.size(); ++dest) {
      world.async(dest, [](int){}, world.rank());
      std::this_thread::sleep_for(std::chrono::milliseconds(world.rank() * 250)); 
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));

    world.barrier();
    if (world.rank0()) {
      std::cout << "Barrier " << i + 1 << "/10\n" << std::flush;
    }
    std::this_thread::sleep_for(std::chrono::seconds(2));


    if (argc > 1 && strcmp(argv[1], "all-trip-oom") == 0){

      std::cout << "Filling vector until oom" << std::endl;
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

    if (argc > 1 && strcmp(argv[1], "trip-seg") == 0){
      std::cout << "Segfault requested." << std::endl;
      int* ptr = nullptr;
      *ptr = 42;

      // TODO: ensure the same behavior for single rank failure      

      /**
       * Local MPIRUN:
       * * winner segfaults and remaining processes are
       *    given a copy of the signal and terminate gracefully.
       * 
       * Cluster SBATCH:
       * 
       * Cluster SRUN:
       * 
       * 
       */
    }
  }
  return 0;
}
