// Copyright 2019-2021 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG

#include <ygm/comm.hpp>
#include <ygm/container/bag.hpp>
#include <ygm/container/map.hpp>
#include <ygm/random/alias_table.hpp>
#include <ygm/random/random.hpp>

int main(int argc, char** argv) {

  ygm::comm world(&argc, &argv);
  using YGM_RNG = ygm::random::default_random_engine<>;
  int seed = 42;
  YGM_RNG ygm_rng(world, seed);

  // Test constructor works with YGM value container
  {
    ygm::container::bag<std::pair<uint32_t,double>> bag_of_items(world);
    // ygm::container::bag<std::tuple<uint32_t,double>> bag_of_items(world);

    uint32_t n_items_per_rank = 1000;
    const int max_item_weight = 100;

    std::uniform_real_distribution<double> dist(0, max_item_weight);
    const uint32_t rank = world.rank();
    for (uint32_t i = 0; i < n_items_per_rank; i++) {
        uint32_t id = i + rank * n_items_per_rank;
        double w = dist(ygm_rng);
        bag_of_items.async_insert({id, w});
    }
    world.barrier();

    ygm::random::alias_table<uint32_t, YGM_RNG> alias_tbl(world, ygm_rng, bag_of_items);

    static uint32_t samples; 
    uint32_t samples_per_rank = 1000;
    for (int i = 0; i < samples_per_rank; i++) {
        alias_tbl.async_sample([](auto ptr, uint32_t item){
            samples++;
        });
    } 
    world.barrier();
    uint32_t total_samples = ygm::sum(samples, world);
    YGM_ASSERT_RELEASE(total_samples == (samples_per_rank * world.size()));
  }

  // Test constructor works with YGM associative container
  {
    ygm::container::map<uint32_t,double> map_of_items(world);

    uint32_t n_items_per_rank = 1000;
    const int max_item_weight = 100;

    std::uniform_real_distribution<double> dist(0, max_item_weight);
    const uint32_t rank = world.rank();
    for (uint32_t i = 0; i < n_items_per_rank; i++) {
        uint32_t id = i + rank * n_items_per_rank;
        double w = dist(ygm_rng);
        map_of_items.async_insert(id, w);
    }
    world.barrier();

    ygm::random::alias_table<uint32_t, YGM_RNG> alias_tbl(world, ygm_rng, map_of_items);

    static uint32_t samples; 
    uint32_t samples_per_rank = 1000;
    for (int i = 0; i < samples_per_rank; i++) {
        alias_tbl.async_sample([](auto ptr, uint32_t item){
            samples++;
        });
    } 
    world.barrier();
    uint32_t total_samples = ygm::sum(samples, world);
    YGM_ASSERT_RELEASE(total_samples == (samples_per_rank * world.size()));
  }

  return 0;
}
