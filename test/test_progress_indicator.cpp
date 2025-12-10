// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#include <chrono>
#include <thread>
#undef NDEBUG

#include <ygm/comm.hpp>
#include <ygm/utility/progress_indicator.hpp>

int main(int argc, char **argv) {
  ygm::comm world(&argc, &argv);

  {
    ygm::utility::progress_indicator prog(world, {.message = "Test 1"});
    for (size_t i = 0; i < 1000; ++i) {
      prog.async_inc();
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    prog.complete();
    world.barrier();
  }

  {
    ygm::utility::progress_indicator prog(world, {.message = "Test 2"});
    for (size_t i = 0; i < 1000; ++i) {
      prog.async_inc();
      world.async_barrier();
    }
    prog.complete();
    world.barrier();
  }

  // Testing Barrier before Complete.   THIS IS NOT IDEAL USAGE
  {
    ygm::utility::progress_indicator prog(world, {.message = "Test 3"});
    for (size_t i = 0; i < 1000 * size_t(world.rank() + 100); ++i) {
      prog.async_inc();
      world.async_barrier();
    }
    world.barrier();
    prog.complete();
  }

  return 0;
}
