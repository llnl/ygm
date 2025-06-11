// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include <ygm/comm.hpp>
#include <ygm/io/parquet_parser.hpp>

int main(int argc, char** argv) {
  ygm::comm world(&argc, &argv);

  world.cout0() << "Arrow Parquet file parser example" << std::endl;

  std::string dir_name = "../test/data/parquet_files/";
  if (argc == 2) {
    dir_name = argv[1];
  }

  // parquet_parser assumes files have identical scehma
  ygm::io::parquet_parser parquetp(world, {dir_name});

  world.cout0() << parquetp.num_files() << " files in " << dir_name
                << std::endl;

  // Print Schema
  world.cout0() << "#of Columns: " << parquetp.get_schema().size() << std::endl;
  world.cout0() << "Schema: " << std::endl;
  for (const auto& col : parquetp.get_schema()) {
    world.cout0() << col.name << ": " << col.type << std::endl;
  }
  world.cout0() << std::endl;
  world.cout0() << "Full schema: " << std::endl;
  world.cout0() << parquetp.schema_to_string() << std::endl;

  world.cout0() << "#of Rows: " << parquetp.num_rows() << std::endl;
  world.cout0() << std::endl;
  world.cf_barrier();

  // Peek first row
  auto row_opt = parquetp.peek();
  if (row_opt.has_value()) {
    std::string buf;
    for (const auto& field : *row_opt) {
      std::visit(
          [&buf](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
              buf += "None, ";
            } else if constexpr (std::is_same_v<T, std::string>) {
              buf += value + ", ";
            } else {
              buf += std::to_string(value) + ", ";
            }
          },
          field);
    }
    world.cout() << "Peek: " << buf << std::endl;
  }
  world.cf_barrier();
  world.cout0() << std::endl;

  // Print values
  for (int r = 0; r < world.size(); ++r) {
    if (r == world.rank()) {
      parquetp.for_all([](const auto& row) {
        // Row is a vector of parquet_parser::parquet_type_variant
        for (const auto& field : row) {
          std::visit(
              [](const auto& value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, std::monostate>) {
                  std::cout << "None" << ", ";
                } else {
                  std::cout << value << ", ";
                }
              },
              field);
        }
        std::cout << std::endl;
      });
    }
    world.cf_barrier();
  }

  return 0;
}
