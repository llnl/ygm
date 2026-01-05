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
  int seed = 42;
  ygm::random::default_random_engine<> ygm_rng(world, seed);

  //
  // Testing various constructors 
  {
    uint32_t n_items_per_rank = 1000;
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
      ygm::random::alias_table<uint32_t> alias_tbl(world, bag_of_items, ygm_rng());
    }
    { // Constructing from ygm::container::map
      ygm::container::map<uint32_t,double> map_of_items(world);
      for (uint32_t i = 0; i < n_items_per_rank; i++) {
        uint32_t id = world.rank() + i * world.size();
        double w = dist(ygm_rng);
        map_of_items.async_insert({id, w});
      }
      world.barrier();
      ygm::random::alias_table<uint32_t> alias_tbl(world, map_of_items, ygm_rng());
    }
    { // Constructing from ygm::container::array
      ygm::container::array<double> array_of_weights(world, n_items_per_rank*world.size());
      for (uint32_t i = 0; i < n_items_per_rank; i++) {
        uint32_t id = world.rank() + i * world.size();
        double w = dist(ygm_rng);
        array_of_weights.async_set(id,w);
      }
      world.barrier();
      ygm::random::alias_table<uint64_t> alias_tbl(world, array_of_weights, ygm_rng());
    }
    { // Constructing from std::vector
      std::vector<std::pair<uint32_t,double>> vec_of_items;
      for (uint32_t i = 0; i < n_items_per_rank; i++) {
        uint32_t id = world.rank() + i * world.size();
        double w = dist(ygm_rng);
        vec_of_items.push_back({id,w});
      }
      world.barrier();
      ygm::random::alias_table<uint32_t> alias_tbl(world, vec_of_items, ygm_rng());
    }
    { // Constructing from std::map
      std::map<uint32_t,double> items_map;
      for (uint32_t i = 0; i < n_items_per_rank; i++) {
        uint32_t id = world.rank() + i * world.size();
        double w = dist(ygm_rng);
        items_map[id] = w;
      }
      world.barrier();
      ygm::random::alias_table<uint32_t> alias_tbl(world, items_map, ygm_rng());
    }
  }

  // 
  // Testing the construction of many distributions. Balancing capabilities being tested.
  {
    uint32_t alias_tables_to_construct = 1000;
    uint32_t n_items_per_rank = 1000;
    { // Testing uniform weight distribution
      std::uniform_int_distribution<uint32_t> max_item_weight_dist(50, 100);
      for (uint32_t i = 0; i < alias_tables_to_construct; i++) {
        // world.cout0("uniform distribution alias table ", i, " of ", alias_tables_to_construct);
        ygm::container::map<uint32_t,double> map_of_items(world);
        uint32_t max_item_weight = max_item_weight_dist(ygm_rng);
        std::uniform_real_distribution<double> weight_dist(0, max_item_weight);
        for (uint32_t j = 0; j < n_items_per_rank; j++) {
          uint32_t id = world.rank() + j * world.size();
          double w = weight_dist(ygm_rng);
          map_of_items.async_insert(id,w);
        }
        world.barrier();
        ygm::random::alias_table<uint32_t> alias_tbl(world, map_of_items);
      }
      world.cout0("Finished uniform distribution alias table test");
    }
    { // Testing normal weight distribution
      std::uniform_int_distribution<uint32_t> mean_dist(50, 100);
      std::uniform_int_distribution<uint32_t> std_dev_dist(5, 20);
      for (uint32_t i = 0; i < alias_tables_to_construct; i++) {
        // world.cout0("Normal distribution alias table ", i, " of ", alias_tables_to_construct);
        ygm::container::map<uint32_t,double> map_of_items(world);
        uint32_t mean = mean_dist(ygm_rng);
        uint32_t std_dev = std_dev_dist(ygm_rng);
        std::normal_distribution<double> weight_dist(mean, std_dev);
        for (uint32_t j = 0; j < n_items_per_rank; j++) {
          uint32_t id = world.rank() + j * world.size();
          double w = weight_dist(ygm_rng);
          map_of_items.async_insert(id,w);
        }
        world.barrier();
        ygm::random::alias_table<uint32_t> alias_tbl(world, map_of_items, ygm_rng());
      }
      world.cout0("Finished normal distribution alias table test");
    }
    { // Testing gamma weight distribution
      std::uniform_real_distribution<double> alpha_dist(0.1, 10);
      std::uniform_real_distribution<double> theta_dist(10, 100);
      for (uint32_t i = 0; i < alias_tables_to_construct; i++) {
        // world.cout0("Gamma distribution alias table ", i, " of ", alias_tables_to_construct);
        ygm::container::map<uint32_t,double> map_of_items(world);
        double alpha = alpha_dist(ygm_rng);
        double theta = theta_dist(ygm_rng);
        std::gamma_distribution<double> weight_dist(alpha, theta);
        for (uint32_t j = 0; j < n_items_per_rank; j++) {
          uint32_t id = world.rank() + j * world.size();
          double w = weight_dist(ygm_rng);
          map_of_items.async_insert(id,w);
        }
        world.barrier();
        ygm::random::alias_table<uint32_t> alias_tbl(world, map_of_items, ygm_rng());
      }
      world.cout0("Finished gamma distribution alias table test");
    }
  }

  //
  // Test sampling numbers
  {
    ygm::container::map<uint32_t,double> map_of_items(world);

    uint32_t n_items_per_rank = 1000;
    uint32_t max_item_weight = 100;
    std::uniform_real_distribution<double> dist(0, max_item_weight);
    for (uint32_t i = 0; i < n_items_per_rank; i++) {
      uint32_t id = world.rank() + i * world.size();
      double w = dist(ygm_rng);
      map_of_items.async_insert(id,w);
    }
    world.barrier();
    ygm::random::alias_table<uint32_t> alias_tbl(world, map_of_items, ygm_rng());

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
    ygm::random::alias_table<std::string> alias_tbl(world, word_counts);
    world.barrier();
    file.close();

    static uint64_t samples = 0; 
    static uint64_t sampled_ipsums = 0;
    static uint64_t sampled_sits = 0;
    uint32_t samples_per_rank = 10000000;
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
    uint64_t total_samples = ygm::sum(samples, world);
    uint64_t total_ipsums = ygm::sum(sampled_ipsums, world);
    uint64_t total_sits = ygm::sum(sampled_sits, world);

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
      YGM_ASSERT_RELEASE(dif < 1e-3);

      world.cout0("\"sit\" actual frequency: ", sit_freq);
      world.cout0("\"sit\" sample frequency: ", sit_sample_freq);
      dif = std::abs(sit_sample_freq - sit_freq);
      world.cout0("\"sit\" frequency difference: ", dif);
      YGM_ASSERT_RELEASE(dif < 1e-3);
    }
  }

  return 0;
}
