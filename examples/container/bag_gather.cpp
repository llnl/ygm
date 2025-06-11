// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#include <ygm/comm.hpp>
#include <ygm/container/bag.hpp>

int main(int argc, char **argv) {
  ygm::comm world(&argc, &argv);

  ygm::container::bag<std::string> bbag(world);
  if (world.rank0()) {
    bbag.async_insert("dog");
    bbag.async_insert("apple");
    bbag.async_insert("red");
  } else if (world.rank() == 1) {
    bbag.async_insert("cat");
    bbag.async_insert("banana");
    bbag.async_insert("blue");
  } else if (world.rank() == 2) {
    bbag.async_insert("fish");
    bbag.async_insert("pear");
    bbag.async_insert("green");
  } else if (world.rank() == 3) {
    bbag.async_insert("snake");
    bbag.async_insert("cherry");
    bbag.async_insert("yellow");
  }
  world.barrier();

  for (int i = 0; i < world.size(); i++) {
    if (i == world.rank()) {
      std::cout << "Rank " << i << std::endl;
      bbag.local_for_all([](std::string &s) { std::cout << s << std::endl; });
      std::cout << std::endl;
    }
    world.barrier();
  }

  world.barrier();
  std::vector<std::string> all_data;
  bbag.gather(all_data, 0);
  if (world.rank0()) {
    for (auto data : all_data) {
      std::cout << data << std::endl;
    }
  }

  world.barrier();
  // std::vector<std::string> all_data_2;
  // bbag.gather(all_data_2);
  // for(int i = 0; i < world.size(); i++) {
  //   if(i == world.rank()) {
  //     std::cout << world.rank() << std::endl;
  //     for(auto data : all_data_2) {
  //       std::cout << data << std::endl;
  //     }
  //   }
  //   world.barrier();
  // }

  return 0;
}
