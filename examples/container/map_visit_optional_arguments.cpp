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
  }

  world.barrier();

  auto visit_lambda = [](auto pmap, auto key, auto value) {
    std::cout << "Rank " << pmap->comm().rank() << " is receiving a lookup\n"
              << "\tKey: " << key << " Value: " << value
              << "\n\tGoing to ask rank 0 to say something." << std::endl;

    // Send message to rank 0 to introduce himself
    pmap->comm().async(
        0,
        [](auto pcomm, int from) {
          std::cout << "Hi. I'm rank " << pcomm->rank() << ". Rank " << from
                    << " wanted me to say something." << std::endl;
        },
        pmap->comm().rank());
  };

  // Send lookup from odd-numbered ranks
  if (world.rank() % 2) {
    my_map.async_visit("dog", visit_lambda);
  }
  return 0;
}
