// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#include <ygm/comm.hpp>

int main(int argc, char **argv) {
  ygm::comm world(&argc, &argv);

  // Define a lambda with optional arguments
  auto opt_args = [](auto pcomm, int msg) {
    std::cout << "Rank " << pcomm->rank() << " received message with contents "
              << msg << " using optional arguments" << std::endl;
  };

  // Define a lambda without optional arguments
  auto no_opt_args = [](int msg) {
    std::cout << "Received message " << msg
              << " without optional arguments. I have no idea who sent this or "
                 "who I am."
              << std::endl;
  };

  if (world.rank() == 0) {
    world.async(1, opt_args, 12);
  }
  if (world.rank() == 1) {
    world.async(0, no_opt_args, 25);
  }
  return 0;
}
