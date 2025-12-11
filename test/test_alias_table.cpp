// Copyright 2019-2021 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG

#include <ygm/comm.hpp>
#include <ygm/container/bag.hpp>
#include <ygm/container/map.hpp>
#include <ygm/container/array.hpp>
#include <ygm/container/counting_set.hpp>
#include <ygm/random/alias_table.hpp>
#include <ygm/random/random.hpp>
#include <map>
#include <vector>
#include <fstream>

int main(int argc, char** argv) {

  ygm::comm world(&argc, &argv);
  using YGM_RNG = ygm::random::default_random_engine<>;
  int seed = 42;
  YGM_RNG ygm_rng(world, seed);

  //
  // Testing various constructors 
  {
    uint32_t n_items_per_rank = 10000;
    const int max_item_weight = 100;
    std::uniform_real_distribution<double> dist(0, max_item_weight);
    { // Constructing from ygm::container::bag
      ygm::container::bag<std::pair<uint32_t,double>> bag_of_items(world);
      for (uint32_t i = 0; i < n_items_per_rank; i++) {
        uint32_t id = world.rank() + i * world.size();
        double w = dist(ygm_rng);
        bag_of_items.async_insert({id, w});
      }
      world.barrier();
      ygm::random::alias_table<uint32_t, YGM_RNG> alias_tbl(world, ygm_rng, bag_of_items);
    }
    { // Constructing from ygm::container::map
      ygm::container::map<uint32_t,double> map_of_items(world);
      for (uint32_t i = 0; i < n_items_per_rank; i++) {
        uint32_t id = world.rank() + i * world.size();
        double w = dist(ygm_rng);
        map_of_items.async_insert({id, w});
      }
      world.barrier();
      ygm::random::alias_table<uint32_t, YGM_RNG> alias_tbl(world, ygm_rng, map_of_items);
    }
    { // Constructing from ygm::container::array
      ygm::container::array<double> array_of_weights(world, n_items_per_rank*world.size());
      for (uint32_t i = 0; i < n_items_per_rank; i++) {
        uint32_t id = world.rank() + i * world.size();
        double w = dist(ygm_rng);
        array_of_weights.async_set(id,w);
      }
      world.barrier();
      ygm::random::alias_table<uint64_t, YGM_RNG> alias_tbl(world, ygm_rng, array_of_weights);
    }
    { // Constructing from std::vector
      std::vector<std::pair<uint32_t,double>> vec_of_items;
      for (uint32_t i = 0; i < n_items_per_rank; i++) {
        uint32_t id = world.rank() + i * world.size();
        double w = dist(ygm_rng);
        vec_of_items.push_back({id,w});
      }
      world.barrier();
      ygm::random::alias_table<uint32_t, YGM_RNG> alias_tbl(world, ygm_rng, vec_of_items);
    }
    { // Constructing from std::map
      std::map<uint32_t,double> items_map;
      for (uint32_t i = 0; i < n_items_per_rank; i++) {
        uint32_t id = world.rank() + i * world.size();
        double w = dist(ygm_rng);
        items_map[id] = w;
      }
      world.barrier();
      ygm::random::alias_table<uint32_t, YGM_RNG> alias_tbl(world, ygm_rng, items_map);
    }
  }

  //
  // Test sampling numbers
  {
    ygm::container::map<uint32_t,double> map_of_items(world);

    uint32_t n_items_per_rank = 10000;
    const int max_item_weight = 100;
    std::uniform_real_distribution<double> dist(0, max_item_weight);
    for (uint32_t i = 0; i < n_items_per_rank; i++) {
      uint32_t id = world.rank() + i * world.size();
      double w = dist(ygm_rng);
      map_of_items.async_insert(id,w);
    }
    world.barrier();
    ygm::random::alias_table<uint32_t, YGM_RNG> alias_tbl(world, ygm_rng, map_of_items);

    static uint32_t samples = 0; 
    uint32_t samples_per_rank = 100000;
    for (uint32_t i = 0; i < samples_per_rank; i++) {
        alias_tbl.async_sample([]([[maybe_unused]] auto ptr, [[maybe_unused]] uint32_t item){ 
          samples++;
        });
    } 
    world.barrier();
    uint32_t total_samples = ygm::sum(samples, world);
    YGM_ASSERT_RELEASE(total_samples == (samples_per_rank * world.size()));
  }

  // 
  // Test sampling words with probability proportional to their frequency in a corpus
  {
    std::vector<std::string> words;
    std::ifstream file("data/loremipsum/loremipsum_0.txt");
    ygm::container::counting_set<std::string> word_counts(world);

    static std::string ipsum = "ipsum";
    uint32_t ipsum_count = 0;
    static std::string sit = "sit";
    uint32_t sit_count = 0;
    uint32_t total_words = 0;
    if (world.rank0()) {
    std::string word;
      while (file >> word) {
        word_counts.async_insert(word); 
        ++total_words;
        if (word == ipsum) {
          ++ipsum_count;
        } else if (word == sit) {
          ++sit_count;
        }
      }
    }
    ygm::random::alias_table<std::string, YGM_RNG> alias_tbl(world, ygm_rng, word_counts);
    world.barrier();
    file.close();

    static uint32_t samples = 0; 
    static uint32_t sampled_ipsums = 0;
    static uint32_t sampled_sits = 0;
    uint32_t samples_per_rank = 100000;
    for (uint32_t i = 0; i < samples_per_rank; i++) {
      alias_tbl.async_sample([](std::string word_sample){
        samples++;
        if (word_sample == ipsum) {
          ++sampled_ipsums;
        } else if (word_sample == sit) {
          ++sampled_sits;
        }
      });
    }
    world.barrier();
    uint32_t total_samples = ygm::sum(samples, world);
    uint32_t total_ipsums = ygm::sum(sampled_ipsums, world);
    uint32_t total_sits = ygm::sum(sampled_sits, world);

    YGM_ASSERT_RELEASE(total_samples == (samples_per_rank * world.size()));

    if (world.rank() == 0) {
      double ipsum_freq = double(ipsum_count) / total_words;
      double sit_freq = double(sit_count) / total_words;
      double ipsum_sample_freq = double(total_ipsums) / total_samples;
      double sit_sample_freq = double(total_sits) / total_samples;

      world.cout0("\"ipsum\" actual frequency: ", ipsum_freq);
      world.cout0("\"ipsum\" sample frequency: ", ipsum_sample_freq);
      double dif = std::abs(ipsum_sample_freq - ipsum_freq);
      world.cout0("\"ipsum\" frequency difference: ", dif);
      YGM_ASSERT_RELEASE(dif < 1e-4);

      world.cout0("\"sit\" actual frequency: ", sit_freq);
      world.cout0("\"sit\" sample frequency: ", sit_sample_freq);
      dif = std::abs(sit_sample_freq - sit_freq);
      world.cout0("\"sit\" frequency difference: ", dif);
      YGM_ASSERT_RELEASE(dif < 1e-4);
    }
  }

  return 0;
}
