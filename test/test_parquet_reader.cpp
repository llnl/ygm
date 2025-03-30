// Copyright 2019-2022 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG

#include <filesystem>
#include <ygm/comm.hpp>
#include <ygm/io/parquet_parser2.hpp>

int main(int argc, char** argv) {
  ygm::comm world(&argc, &argv);

  //
  // Test number of lines in files
  {
    // assuming the build directory is inside the YGM root directory
    const std::string dir_name = "data/parquet_files/";

    // parquet_parser assumes files have identical scehma
    ygm::io::parquet_parser parquetp(world, {dir_name});

    // count total number of rows in files
    size_t local_count = 0;

    parquetp.for_all(
        [&local_count](const auto& read_values) {
          local_count++;
        });

    world.barrier();
    auto row_count = world.all_reduce_sum(local_count);
    YGM_ASSERT_RELEASE(row_count == 12);
  }

  //
  // Test table entries
  {
    // assuming the build directory is inside the YGM root directory
    const std::string dir_name = "data/parquet_files/";

    // parquet_parser assumes files have identical scehma
    ygm::io::parquet_parser parquetp(world, {dir_name});

    // read fields in each row
    struct columns {
      std::string string_field;
      char        char_array_field[4];
      int64_t    int64_field;
      double      double_field;
      bool        boolean_field;
    };

    std::vector<columns>  rows;
    std::set<std::string> strings;

    parquetp.for_all(
        [&rows, &strings](const auto& read_values) {
          columns data;
          data.string_field = std::get<std::string>(read_values[0]);
          data.int64_field = std::get<int64_t>(read_values[1]);
          data.double_field = std::get<double>(read_values[2]);
          data.boolean_field = std::get<bool>(read_values[3]);

          rows.emplace_back(data);

          strings.insert(data.string_field);
        });

    world.barrier();
    auto row_count = world.all_reduce_sum(rows.size());
    YGM_ASSERT_RELEASE(row_count == 12);

    YGM_ASSERT_RELEASE(world.all_reduce_sum(strings.count("Hennessey Venom F5")) ==
                   1);
  }

  return 0;
}
