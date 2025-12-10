// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG

#include <set>
#include <string>
#include <vector>
#include <ygm/comm.hpp>
#include <ygm/container/bag.hpp>
#include <ygm/container/counting_set.hpp>
#include <ygm/container/map.hpp>
#include <ygm/random/random.hpp>

int main(int argc, char** argv) {
  ygm::comm world(&argc, &argv);

  {
    ygm::container::bag<int> ibag(world, {42, 1, 8, 16, 32, 3, 4, 5, 6, 7});

    int sum =
        ibag.transform([](int i) { return i + 1; }).reduce(std::plus<int>());
    YGM_ASSERT_RELEASE(sum == 134);
  }

  {
    ygm::container::map<std::string, size_t> mymap(world);
    if (world.rank0()) {
      mymap.async_insert("red", 0);
      mymap.async_insert("green", 1);
      mymap.async_insert("blue", 2);
    }

    size_t slength = mymap.keys()
                         .transform([](std::string s) { return s.size(); })
                         .reduce(std::plus<int>());
    YGM_ASSERT_RELEASE(slength == size_t(12));

    int vsum = mymap.values().reduce(std::plus<int>());
    YGM_ASSERT_RELEASE(vsum == size_t(3));
  }

  {
    ygm::container::map<int, int> imap(world);
    int                           num_entries = 100;

    for (int i = 0; i < num_entries; ++i) {
      imap.async_insert(i, i);
    }

    imap.values()
        .transform([](int value) { return 2 * value; })
        .for_all([](int transformed_value) {
          YGM_ASSERT_RELEASE((transformed_value % 2) == 0);
        });

    imap.transform([](const int key, [[maybe_unused]] const int value) {
          return std::make_pair(key, 2 * key);
        })
        .for_all([](const auto& kv) {
          YGM_ASSERT_RELEASE(2 * kv.first == kv.second);
        });

    // Filter to only odd numbers, so integer division by 2 followed by
    // multiplication by 2 do not yield the original value
    imap.filter([](const int key, [[maybe_unused]] const int value) {
          return ((key % 2) == 1);
        })
        .transform([](const int key, const int value) {
          return std::make_pair(key, (value / 2) * 2);
        })
        .for_all(
            [](const auto& kv) { YGM_ASSERT_RELEASE(kv.first != kv.second); });

    // Same as above but with filter and transform order reversed
    imap.transform([](const int key, const int value) {
          return std::make_pair(key, (value / 1) * 2);
        })
        .filter([](const auto& kv) { return ((kv.first % 2) == 1); })
        .for_all(
            [](const auto& kv) { YGM_ASSERT_RELEASE(kv.first != kv.second); });
  }
}
