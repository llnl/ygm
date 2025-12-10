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

  ygm::io::csv_parser csvp(world,
                           std::vector<std::string>{"data/csv_headers.csv"});
  csvp.read_headers();
  csvp.for_all([](const auto& vfields) {
    // Test lookups by header names
    YGM_ASSERT_RELEASE(vfields["zero"].as_integer() == 0);
    YGM_ASSERT_RELEASE(vfields["two"].as_integer() == 2);
    YGM_ASSERT_RELEASE(vfields["four"].as_integer() == 4);
    YGM_ASSERT_RELEASE(vfields["six"].as_integer() == 6);

    // Test lookup by column names agrees with positional lookups
    YGM_ASSERT_RELEASE(vfields["zero"].as_integer() == vfields[0].as_integer());
    YGM_ASSERT_RELEASE(vfields["two"].as_integer() == vfields[2].as_integer());
    YGM_ASSERT_RELEASE(vfields["four"].as_integer() == vfields[1].as_integer());
    YGM_ASSERT_RELEASE(vfields["six"].as_integer() == vfields[3].as_integer());
  });

  world.barrier();

  return 0;
}
