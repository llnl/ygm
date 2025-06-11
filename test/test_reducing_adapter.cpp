// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG

#include <string>
#include <ygm/comm.hpp>
#include <ygm/container/array.hpp>
#include <ygm/container/detail/reducing_adapter.hpp>
#include <ygm/container/map.hpp>

int main(int argc, char **argv) {
  ygm::comm world(&argc, &argv);

  //
  // Test reducing_adapter on ygm::map
  {
    ygm::container::map<std::string, int> test_map(world);
    int                                   num_reductions = 6;

    {
      auto reducing_map = ygm::container::detail::make_reducing_adapter(
          test_map,
          [](const int &a, const int &b) { return std::max<int>(a, b); });

      for (int i = 0; i < num_reductions; ++i) {
        reducing_map.async_reduce("max", i);
      }
    }

    {
      auto reducing_map = ygm::container::detail::make_reducing_adapter(
          test_map, std::plus<int>());

      for (int i = 0; i < num_reductions; ++i) {
        reducing_map.async_reduce("sum", i);
      }
    }

    world.barrier();

    test_map.for_all(
        [&num_reductions, &world](const auto &key, const auto &value) {
          if (key == "max") {
            YGM_ASSERT_RELEASE(value == num_reductions - 1);
          } else if (key == "sum") {
            YGM_ASSERT_RELEASE(value == world.size() * num_reductions *
                                        (num_reductions - 1) / 2);
          } else {
            YGM_ASSERT_RELEASE(false);
          }
        });
  }

  //
  // Test reducing_adapter on ygm::array
  {
    ygm::container::array<int> test_array(world, 10);

    auto reducing_array = ygm::container::detail::make_reducing_adapter(
        test_array,
        [](const int &a, const int &b) { return std::max<int>(a, b); });

    int num_reductions = 6;
    for (int i = 0; i < num_reductions; ++i) {
      reducing_array.async_reduce(0, i);
    }

    test_array.for_all([&num_reductions](const auto &index, const auto &value) {
      if (index == 0) {
        YGM_ASSERT_RELEASE(value == num_reductions - 1);
      } else {
        YGM_ASSERT_RELEASE(value == 0);
      }
    });
  }

  return 0;
}
