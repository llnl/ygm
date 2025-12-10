// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG

#include <filesystem>
#include <ygm/comm.hpp>
#include <ygm/io/csv_parser.hpp>

int main(int argc, char** argv) {
  ygm::comm world(&argc, &argv);

  size_t              local_count{0};
  ygm::io::csv_parser csvp(world, std::vector<std::string>{"data/100.csv"});
  csvp.for_all([&local_count](const auto& vfields) {
    for (auto f : vfields) {
      YGM_ASSERT_RELEASE(f.is_integer());
      local_count += f.as_integer();
    }
  });

  world.barrier();
  YGM_ASSERT_RELEASE(ygm::sum(local_count, world) == 100);

  return 0;
}
