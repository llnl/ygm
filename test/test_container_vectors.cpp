// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG

#include <ygm/comm.hpp>
#include <ygm/container/array.hpp>
#include <ygm/container/bag.hpp>
#include <ygm/container/counting_set.hpp>
#include <ygm/container/map.hpp>
#include <ygm/container/set.hpp>

int main(int argc, char** argv) {
  ygm::comm world(&argc, &argv);

  {
    std::vector<ygm::container::array<int>> vec_arrays;

    size_t size = 1024;
    vec_arrays.emplace_back(world, size, 0);
    vec_arrays.emplace_back(world, size, 0);

    if (world.rank0()) {
      for (size_t i = 0; i < size; ++i) {
        vec_arrays[1].async_set(i, i);
      }
    }

    vec_arrays[0].for_all(
        []([[maybe_unused]] const auto index, const auto value) {
          YGM_ASSERT_RELEASE(value == size_t(0));
        });
    vec_arrays[1].for_all([](const auto index, const auto value) {
      YGM_ASSERT_RELEASE(index == size_t(value));
    });
  }

  {
    std::vector<ygm::container::bag<std::string>> vec_bags;

    vec_bags.emplace_back(world);
    vec_bags.emplace_back(world);

    if (world.rank0()) {
      vec_bags[1].async_insert("dog");
      vec_bags[1].async_insert("apple");
      vec_bags[1].async_insert("red");
    }
    YGM_ASSERT_RELEASE(vec_bags[0].size() == 0);
    YGM_ASSERT_RELEASE(vec_bags[1].count("dog") == 1);
    YGM_ASSERT_RELEASE(vec_bags[1].count("apple") == 1);
    YGM_ASSERT_RELEASE(vec_bags[1].count("red") == 1);
    YGM_ASSERT_RELEASE(vec_bags[1].size() == 3);

    // test contains.
    YGM_ASSERT_RELEASE(vec_bags[1].contains("dog"));
    YGM_ASSERT_RELEASE(vec_bags[1].contains("apple"));
    YGM_ASSERT_RELEASE(vec_bags[1].contains("red"));
    YGM_ASSERT_RELEASE(!vec_bags[1].contains("blue"));
  }

  {
    std::vector<ygm::container::counting_set<std::string>> vec_csets;

    vec_csets.emplace_back(world);
    vec_csets.emplace_back(world);

    if (world.rank() == 0) {
      vec_csets[1].async_insert("dog");
      vec_csets[1].async_insert("apple");
      vec_csets[1].async_insert("red");
    }

    YGM_ASSERT_RELEASE(vec_csets[0].size() == 0);
    YGM_ASSERT_RELEASE(vec_csets[1].count("dog") == 1);
    YGM_ASSERT_RELEASE(vec_csets[1].count("apple") == 1);
    YGM_ASSERT_RELEASE(vec_csets[1].count("red") == 1);
    YGM_ASSERT_RELEASE(vec_csets[1].size() == 3);
  }

  {
    std::vector<ygm::container::map<std::string, std::string>> vec_maps;

    vec_maps.emplace_back(world);
    vec_maps.emplace_back(world);

    if (world.rank() == 0) {
      vec_maps[1].async_insert("dog", "cat");
      vec_maps[1].async_insert("apple", "orange");
      vec_maps[1].async_insert("red", "green");
    }
    YGM_ASSERT_RELEASE(vec_maps[0].size() == 0);
    YGM_ASSERT_RELEASE(vec_maps[1].count("dog") == 1);
    YGM_ASSERT_RELEASE(vec_maps[1].count("apple") == 1);
    YGM_ASSERT_RELEASE(vec_maps[1].count("red") == 1);
    YGM_ASSERT_RELEASE(vec_maps[1].size() == 3);

    // test contains.
    YGM_ASSERT_RELEASE(vec_maps[1].contains("dog"));
    YGM_ASSERT_RELEASE(vec_maps[1].contains("apple"));
    YGM_ASSERT_RELEASE(vec_maps[1].contains("red"));
    YGM_ASSERT_RELEASE(!vec_maps[1].contains("blue"));
  }

  {
    std::vector<ygm::container::set<std::string>> vec_sets;

    vec_sets.emplace_back(world);
    vec_sets.emplace_back(world);

    if (world.rank() == 0) {
      vec_sets[1].async_insert("dog");
      vec_sets[1].async_insert("apple");
      vec_sets[1].async_insert("red");
    }
    YGM_ASSERT_RELEASE(vec_sets[0].size() == 0);
    YGM_ASSERT_RELEASE(vec_sets[1].count("dog") == 1);
    YGM_ASSERT_RELEASE(vec_sets[1].count("red") == 1);
    YGM_ASSERT_RELEASE(vec_sets[1].count("apple") == 1);
    YGM_ASSERT_RELEASE(vec_sets[1].size() == 3);

    // test contains.
    YGM_ASSERT_RELEASE(vec_sets[1].contains("dog"));
    YGM_ASSERT_RELEASE(vec_sets[1].contains("apple"));
    YGM_ASSERT_RELEASE(vec_sets[1].contains("red"));
    YGM_ASSERT_RELEASE(!vec_sets[1].contains("blue"));
  }

  return 0;
}
