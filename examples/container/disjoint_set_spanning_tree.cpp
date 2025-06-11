// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#include <vector>
#include <ygm/comm.hpp>
#include <ygm/container/disjoint_set.hpp>

// Finds a spanning tree on a small graph
int main(int argc, char **argv) {
  ygm::comm world(&argc, &argv);

  std::vector<std::pair<int, int>> graph_edges = {
      {0, 1}, {1, 2}, {1, 3}, {0, 3}, {2, 4}, {2, 5}, {3, 5}, {4, 5}};

  world.cout0("---Graph edges---");
  for (const auto &edge : graph_edges) {
    world.cout0("(", edge.first, ", ", edge.second, ")");
  }

  static std::vector<std::pair<int, int>> local_spanning_tree_edges;

  ygm::container::disjoint_set<int> dset(world);

  auto add_spanning_tree_edges_lambda = [](const int u, const int v,
                                           const bool union_result) {
    if (union_result) {
      local_spanning_tree_edges.push_back(std::make_pair(u, v));
    }
  };

  for (const auto &[u, v] : graph_edges) {
    dset.async_union_and_execute(u, v, add_spanning_tree_edges_lambda);
  }

  world.barrier();

  world.cout0("\n---Spanning tree edges---");
  for (const auto &spanning_tree_edge : local_spanning_tree_edges) {
    world.cout() << "(" << spanning_tree_edge.first << ", "
                 << spanning_tree_edge.second << ")" << std::endl;
  }

  return 0;
}
