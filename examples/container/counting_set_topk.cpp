// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#include <ygm/comm.hpp>
#include <ygm/container/counting_set.hpp>

int main(int argc, char **argv) {
  ygm::comm world(&argc, &argv);

  ygm::container::counting_set<std::string> cset(world);

  // Insert from all ranks
  cset.async_insert("dog");
  cset.async_insert("dog");
  cset.async_insert("dog");
  cset.async_insert("cat");
  cset.async_insert("cat");
  cset.async_insert("bird");

  auto topk = cset.topk(
      2, [](const auto &a, const auto &b) { return a.second > b.second; });

  std::for_each(topk.begin(), topk.end(), [&world](const auto &p) {
    world.cout0(p.first, ": ", p.second);
  });

  return 0;
}
