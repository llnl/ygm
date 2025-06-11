// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#include <vector>
#include <ygm/comm.hpp>
#include <ygm/container/disjoint_set.hpp>

// Performs unions and prints a message when sets merge
int main(int argc, char **argv) {
  ygm::comm world(&argc, &argv);

  ygm::container::disjoint_set<int> dset(world);

  auto union_lambda = [](const int a, const int b, const bool union_result,
                         const int originator) {
    if (union_result) {
      std::cout << a << " and " << b << " union occurred originating from "
                << originator << std::endl;
    }
  };

  if (world.rank() % 2) {
    dset.async_union_and_execute(0, 1, union_lambda, world.rank());
    dset.async_union_and_execute(0, 2, union_lambda, world.rank());
    dset.async_union_and_execute(1, 2, union_lambda, world.rank());
  } else {
    dset.async_union_and_execute(3, 4, union_lambda, world.rank());
  }

  world.barrier();

  return 0;
}
