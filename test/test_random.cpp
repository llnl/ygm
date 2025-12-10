// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG
#include <ygm/comm.hpp>
#include <ygm/container/counting_set.hpp>
#include <ygm/random/random.hpp>

#include <random>

int main(int argc, char** argv) {
  ygm::comm world(&argc, &argv);

  //
  // Test default_random_engine
  {
    std::uint32_t                     seed = 100;
    ygm::random::default_random_engine<>      rng(world, seed);
    ygm::container::counting_set<int> seed_set(world);
    ygm::container::counting_set<int> rn_set(world);
    ygm::container::counting_set<int> sample_set(world);

    std::uint32_t                                local_rn = rng();
    std::uniform_int_distribution<std::uint32_t> dist(0, 10000000);
    std::uint32_t                                local_sample = dist(rng);
    seed_set.async_insert(rng.seed());
    rn_set.async_insert(local_rn);
    sample_set.async_insert(local_sample);
    world.barrier();

    int local_counter(0);
    seed_set.for_all([&local_counter]([[maybe_unused]] int key, int val) {
      YGM_ASSERT_RELEASE(val == 1);
      ++local_counter;
    });

    // this can fail if two samples collide, but that is very unlikely.
    // is it worth the trouble of making the test more robust?
    rn_set.for_all([]([[maybe_unused]] int key, int val) {
      YGM_ASSERT_RELEASE(val == 1);
    });
    sample_set.for_all([]([[maybe_unused]] int key, int val) {
      YGM_ASSERT_RELEASE(val == 1);
    });

    int global_counter = ygm::sum(local_counter, world);

    YGM_ASSERT_RELEASE(global_counter == world.size());
  }
}
