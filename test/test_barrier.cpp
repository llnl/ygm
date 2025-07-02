// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG

#include <ygm/comm.hpp>

int main(int argc, char **argv) {
  ygm::comm world(&argc, &argv);

  // Test barriers for early exit
  {
    int        num_rounds = 100;
    static int round      = 0;
    for (int i = 0; i < num_rounds; ++i) {
      world.async_bcast(
          [](int curr_round) { YGM_ASSERT_RELEASE(curr_round == round); },
          round);

      world.barrier();

      ++round;
    }
  }

  // Tests async_barriers
  int        num_rounds = 100;
  static int round      = 0;
  for (int i = 0; i < num_rounds; ++i) {
    world.async_bcast(
        [](int curr_round) { YGM_ASSERT_RELEASE(curr_round == round); }, round);

    for (int j = 0; j < world.rank();
         ++j) {  // each rank calls 'rank' number of async_barriers
      world.async_barrier();
    }
    world.barrier();

    ++round;
  }

  return 0;
}
