// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#include <ygm/comm.hpp>
#include <ygm/container/map.hpp>

int main(int argc, char **argv) {
  ygm::comm world(&argc, &argv);

  ygm::container::multimap<std::string, std::string> my_multimap(world);

  if (world.rank0()) {
    my_multimap.async_insert("dog", "bark");
    my_multimap.async_insert("dog", "woof");
  }

  world.barrier();

  world.cout0("Visiting individual key-value pairs with async_visit");

  // async_visit gives access to individual key-value pairs
  auto visit_lambda = [](const auto &key, const auto &value) {
    std::cout << "One thing a " << key << " says is " << value << std::endl;
  };

  if (world.rank() % 2) {
    my_multimap.async_visit("dog", visit_lambda);
  }

  world.barrier();

  world.cout0("Visiting key-value pairs for key 'dog' as a group");

  // async_visit_group provides begin and end iterators to the collection of
  // key-value pairs associated to a single key
  auto visit_group_lambda = [](auto begin_iter, auto end_iter) {
    std::cout << "The " << begin_iter->first << " says " << begin_iter->second;

    for (auto curr_iter = ++begin_iter; curr_iter != end_iter; ++curr_iter) {
      std::cout << " or " << curr_iter->second;
    }
    std::cout << std::endl;
  };

  // Send lookup from odd-numbered ranks
  if (world.rank() % 2) {
    my_multimap.async_visit_group("dog", visit_group_lambda);
  }
  return 0;
}
