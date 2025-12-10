// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG

#include <ygm/comm.hpp>

int main(int argc, char** argv) {
  ygm::comm world(&argc, &argv);

  YGM_ASSERT_RELEASE(ygm::sum(size_t(1), world) == size_t(world.size()));
  YGM_ASSERT_RELEASE(ygm::sum(double(1), world) == double(world.size()));
  YGM_ASSERT_RELEASE(ygm::sum(float(1), world) == float(world.size()));

  YGM_ASSERT_RELEASE(ygm::min(world.rank(), world) == 0);
  YGM_ASSERT_RELEASE(ygm::min(double(world.rank()), world) == double(0));
  YGM_ASSERT_RELEASE(ygm::min(float(world.rank()), world) == float(0));

  YGM_ASSERT_RELEASE(ygm::max(world.rank(), world) == world.size() - 1);

  YGM_ASSERT_RELEASE(ygm::prefix_sum(1, world) == world.rank());

  YGM_ASSERT_RELEASE(ygm::logical_and(true, world) == true);
  YGM_ASSERT_RELEASE(ygm::logical_and(false, world) == false);
  YGM_ASSERT_RELEASE(ygm::logical_or(true, world) == true);
  YGM_ASSERT_RELEASE(ygm::logical_or(false, world) == false);

  if (world.size() > 1) {
    YGM_ASSERT_RELEASE(ygm::logical_and(world.rank() % 2 == 0, world) == 0);
    YGM_ASSERT_RELEASE(ygm::logical_or(world.rank() % 2 == 0, world) == 1);
  }

  {
    double value(0);
    int    root = 0;
    if (world.rank() == root) {
      value = 3.14;
    }
    ygm::bcast(value, root, world);
    YGM_ASSERT_RELEASE(value == 3.14);
  }

  {
    if (world.size() > 3) {
      size_t value(3);
      int    root = 3;
      if (world.rank() == root) {
        value = 42;
      }
      ygm::bcast(value, root, world);
      YGM_ASSERT_RELEASE(value == 42);
    }
  }

  YGM_ASSERT_RELEASE(is_same(42, world));
  std::set<std::string> string_set;
  if (world.rank() == 0) {
    string_set.insert("Howdy");
    string_set.insert("Aggs");
  }

  if (world.size() > 1) {
    YGM_ASSERT_RELEASE(not is_same(string_set, world));
  }
  string_set.insert("Howdy");
  string_set.insert("Aggs");
  YGM_ASSERT_RELEASE(is_same(string_set, world));

  if (world.size() > 1) {
    YGM_ASSERT_RELEASE(not is_same(world.rank(), world));
  }

  return 0;
}
