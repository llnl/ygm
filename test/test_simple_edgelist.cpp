// Copyright 2019-2026 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#include <vector>
#undef NDEBUG

#include <ygm/comm.hpp>
#include <ygm/graph/directed_multi_edgelist.hpp>
#include <ygm/graph/directed_simple_edgelist.hpp>
#include <ygm/graph/undirected_multi_edgelist.hpp>
#include <ygm/graph/undirected_simple_edgelist.hpp>
#include <ygm/io/csv_parser.hpp>
#include <ygm/utility/assert.hpp>

int main(int argc, char** argv) {
  ygm::comm world(&argc, &argv);

  ygm::graph::undirected_simple_edgelist<int> el(world);
  el.async_insert_or_assign(1, 2);
  el.async_insert_or_assign(2, 3);
  el.async_insert_or_assign(3, 4);
  el.async_insert_or_assign(4, 1);
  YGM_ASSERT_RELEASE(el.size() == 4);

  {
    ygm::io::csv_parser csvp(
        world, std::vector<std::string>{"data/graph/simple/star10_a.csv"});
    csvp.for_all([](const auto& vfields) {
      YGM_ASSERT_RELEASE(vfields.size() == 2);
      YGM_ASSERT_RELEASE(vfields[0].is_integer());
      YGM_ASSERT_RELEASE(vfields[1].is_integer());
      auto u = vfields[0].as_integer();
      auto v = vfields[1].as_integer();
    });
  }

  return 0;
}