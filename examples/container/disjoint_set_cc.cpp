// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#include <ygm/comm.hpp>
#include <ygm/container/disjoint_set.hpp>

int main(int argc, char** argv) {
  ygm::comm world(&argc, &argv);

  // Edges of a social network in which friends must share the same first letter
  // in their names
  std::vector<std::pair<std::string, std::string>> edges = {{"Alice", "Alfred"},
                                                            {"Alfred", "Anne"},
                                                            {"Bob", "Beth"},
                                                            {"Beth", "Beverly"},
                                                            {"Beth", "Bert"}};

  ygm::container::disjoint_set<std::string> connected_components(world);

  for (const auto& friend_pair : edges) {
    world.cout0(friend_pair.first, " is friends with ", friend_pair.second);
  }

  world.cout0("\nPerforming unions on all edges");

  if (world.rank0()) {
    for (const auto& friend_pair : edges) {
      connected_components.async_union(friend_pair.first, friend_pair.second);
    }
  }

  world.cout0(
      "Compressing connected_components to find each person's friend circle "
      "representative\n");
  connected_components.all_compress();

  world.cout0("Person : Representative");
  connected_components.for_all([&world](const auto& person, const auto& rep) {
    std::cout << person << " : " << rep << std::endl;
  });
}
