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

int main(int argc, char** argv) {
  ygm::comm world(&argc, &argv);

  {
    ygm::container::bag<int> ibag(world, {42, 1, 8, 16, 32, 3, 4, 5, 6, 7});

    auto top2 = ibag.gather_topk(2);

    YGM_ASSERT_RELEASE(top2[0] == 42);
    YGM_ASSERT_RELEASE(top2[1] == 32);
    YGM_ASSERT_RELEASE(top2.size() == 2);
  }

  {
    ygm::container::counting_set<std::string> cs(world);
    cs.async_insert("one");
    cs.async_insert("fish");
    cs.async_insert("two");
    cs.async_insert("fish");
    cs.async_insert("red");
    cs.async_insert("fish");
    cs.async_insert("blue");
    cs.async_insert("fish");

    std::vector<std::pair<std::string, size_t> > top1 = cs.gather_topk(
        1, [](auto p1, auto p2) { return p1.second > p2.second; });

    YGM_ASSERT_RELEASE(top1[0].first == "fish");
    YGM_ASSERT_RELEASE(top1[0].second == size_t(4 * world.size()));
  }
}
