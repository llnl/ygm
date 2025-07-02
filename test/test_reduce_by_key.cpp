// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG

#include <ygm/comm.hpp>
#include <ygm/container/bag.hpp>
#include <ygm/container/reduce_by_key.hpp>

int main(int argc, char** argv) {
  ygm::comm world(&argc, &argv);

  {
    ygm::container::bag<std::pair<int, int>> mybag(world);

    mybag.async_insert({0, 1});

    auto test = ygm::container::reduce_by_key_map<int, int>(
        mybag, [](int a, int b) { return a + b; }, world);

    YGM_ASSERT_RELEASE(test.size() == 1);
    test.async_visit(
        0,
        [](int key, int value, int size) { YGM_ASSERT_RELEASE(value == size); },
        world.size());
  }

  {
    std::vector<std::pair<std::string, size_t>> vec_str_count;

    vec_str_count.push_back({"Howdy", 1});
    vec_str_count.push_back({"Aggs", 2});

    auto test = ygm::container::reduce_by_key_map<std::string, size_t>(
        vec_str_count, [](size_t a, size_t b) { return a + b; }, world);

    YGM_ASSERT_RELEASE(test.size() == 2);

    size_t found = 0;
    test.for_all([&found, &world](const std::string& s, size_t c) {
      if (s == "Howdy") {
        ++found;
        YGM_ASSERT_RELEASE(c == world.size());
      } else if (s == "Aggs") {
        ++found;
        YGM_ASSERT_RELEASE(c == world.size() * 2);
      } else {
        YGM_ASSERT_RELEASE(false);
      }
    });
    YGM_ASSERT_RELEASE(ygm::sum(found, world) == 2);
  }

  return 0;
}
