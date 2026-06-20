// Copyright 2019-2026 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG

#include <ygm/comm.hpp>
#include <ygm/graph/undirected_simple_edgelist.hpp>
#include <ygm/graph/undirected_static_graph.hpp>

int main(int argc, char** argv) {
  ygm::comm world(&argc, &argv);

  ygm::graph::undirected_simple_edgelist<int> el(world);
  el.async_insert(1, 2);
  el.async_insert(2, 3);
  el.async_insert(3, 4);
  el.async_insert(4, 1);

  ygm::graph::undirected_static_graph<int> g(world, el);

  return 0;
}