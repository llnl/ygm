// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#include <ranges>
#undef NDEBUG

#include <ygm/comm.hpp>
#include <ygm/container/array.hpp>
#include <ygm/container/bag.hpp>
#include <ygm/container/set.hpp>

int main(int argc, char** argv) {
  ygm::comm world(&argc, &argv);

  ygm::container::bag<int> ibag(world, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
  YGM_ASSERT_RELEASE(ibag.size() == 10);

  ygm::container::bag<int> filtered_bag(
      world, ibag | std::views::filter([](int i) { return i % 2 == 0; }));
  YGM_ASSERT_RELEASE(filtered_bag.size() == 5);

  ygm::container::set<int> filtered_set(
      world, ibag | std::views::filter([](int i) { return i % 2 == 0; }));
  YGM_ASSERT_RELEASE(filtered_set.size() == 5);

  ygm::container::array<int> filtered_array(
      world, ibag | std::views::filter([](int i) { return i % 2 == 0; }));
  YGM_ASSERT_RELEASE(filtered_array.size() == 5);

  ygm::container::set<int> filtered_bag_set(world, filtered_bag);
  YGM_ASSERT_RELEASE(filtered_bag_set.size() == 5);

  // We should fix this
  //   ygm::container::set<int> filtered_array_set(
  //       world, filtered_array | std::views::transform([](const auto& pair)
  //       {
  //                return pair.second;
  //              }));
  //   YGM_ASSERT_RELEASE(filtered_array_set.size() == 5);

  // We should fix this
  // YGM_ASSERT_RELEASE(filtered_bag_set == filtered_set);
}