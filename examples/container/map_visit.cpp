// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#include <ygm/comm.hpp>
#include <ygm/container/map.hpp>

int main(int argc, char **argv) {
  ygm::comm world(&argc, &argv);

  ygm::container::map<std::string, std::string> my_map(world);

  if (world.rank0()) {
    my_map.async_insert("dog", "bark");
    my_map.async_insert("cat", "meow");
  }

  world.barrier();

  auto favorites_lambda = [](auto key, auto &value, const int favorite_num) {
    std::cout << "My favorite animal is a " << key << ". It says '" << value
              << "!' My favorite number is " << favorite_num << std::endl;
  };

  // Send visitors to map
  if (world.rank() % 2) {
    my_map.async_visit("dog", favorites_lambda, world.rank());
  } else {
    my_map.async_visit("cat", favorites_lambda, world.rank() + 1000);
  }

  return 0;
}
