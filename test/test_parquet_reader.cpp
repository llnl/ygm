// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG

#include <filesystem>
#include <ygm/comm.hpp>
#include <ygm/io/parquet_parser.hpp>

int main(int argc, char** argv) {
  ygm::comm world(&argc, &argv);

  // assuming the build directory is inside the YGM root directory
  const std::string dir_name = "data/parquet_files/";

  //
  // Test number of columns and rows in files
  {
    // parquet_parser assumes files have identical scehma
    ygm::io::parquet_parser parquetp(world, {dir_name});

    YGM_ASSERT_RELEASE(parquetp.num_files() == 3);
    YGM_ASSERT_RELEASE(parquetp.num_rows() == 10);
    YGM_ASSERT_RELEASE(parquetp.get_schema().size() == 6);

    struct Row {
      int32_t int32_col;
      int64_t int64_col;
      float   float_col;
      double  double_col;
      bool    bool_col;
    };

    std::unordered_map<std::string, Row> expected_data_table = {
        {"apple", {1, 10, 1.1f, 10.01, true}},
        {"banana", {2, 20, 2.2f, 20.02, false}},
        {"cherry", {3, 30, 3.3f, 30.03, true}},
        {"date", {4, 40, 4.4f, 40.04, false}},
        {"elderberry", {5, 50, 5.5f, 50.05, true}},
        {"fig", {6, 60, 6.6f, 60.06, false}},
        {"grape", {7, 70, 7.7f, 70.07, true}},
        {"honeydew", {8, 80, 8.8f, 80.08, false}},
        {"kiwi", {9, 90, 9.9f, 90.09, true}},
        {"lemon", {10, 100, 10.1f, 100.10, false}}};

    size_t count_rows = 0;
    parquetp.for_all(
        [&expected_data_table, &count_rows](const auto& read_values) {
          const auto key        = std::get<std::string>(read_values[0]);
          const auto int32_col  = std::get<int32_t>(read_values[1]);
          const auto int64_col  = std::get<int64_t>(read_values[2]);
          const auto float_col  = std::get<float>(read_values[3]);
          const auto double_col = std::get<double>(read_values[4]);
          const auto bool_col   = std::get<bool>(read_values[5]);

          const auto& expected = expected_data_table[key];
          YGM_ASSERT_RELEASE(int32_col == expected.int32_col);
          YGM_ASSERT_RELEASE(int64_col == expected.int64_col);
          YGM_ASSERT_RELEASE(float_col == expected.float_col);
          YGM_ASSERT_RELEASE(double_col == expected.double_col);
          YGM_ASSERT_RELEASE(bool_col == expected.bool_col);
          ++count_rows;
        });
    YGM_ASSERT_RELEASE(ygm::sum(count_rows, world) == 10);

    // For all with column names
    count_rows = 0;
    parquetp.for_all(
        {"int64_col", "float_col", "string_col", "int64_col"},
        [&expected_data_table, &count_rows](const auto& read_values) {
          const auto int64_col  = std::get<int64_t>(read_values[0]);
          const auto float_col  = std::get<float>(read_values[1]);
          const auto key        = std::get<std::string>(read_values[2]);
          const auto int64_col2 = std::get<int64_t>(read_values[3]);

          const auto& expected = expected_data_table[key];
          YGM_ASSERT_RELEASE(int64_col == expected.int64_col);
          YGM_ASSERT_RELEASE(float_col == expected.float_col);
          YGM_ASSERT_RELEASE(int64_col2 == expected.int64_col);

          ++count_rows;
        });
    YGM_ASSERT_RELEASE(ygm::sum(count_rows, world) == 10);

    // Test peek()
    {
      ygm::io::parquet_parser parquetp(world, {dir_name});
      const auto              row_opt = parquetp.peek();
      if (row_opt.has_value()) {
        const auto& row = *row_opt;
        YGM_ASSERT_RELEASE(row.size() == 6);
        auto&       key      = std::get<std::string>(row[0]);
        const auto& expected = expected_data_table[key];
        YGM_ASSERT_RELEASE(std::get<int32_t>(row[1]) == expected.int32_col);
        YGM_ASSERT_RELEASE(std::get<int64_t>(row[2]) == expected.int64_col);
        YGM_ASSERT_RELEASE(std::get<float>(row[3]) == expected.float_col);
        YGM_ASSERT_RELEASE(std::get<double>(row[4]) == expected.double_col);
        YGM_ASSERT_RELEASE(std::get<bool>(row[5]) == expected.bool_col);
      }
      world.cf_barrier();

      // Make sure every processes read diffrent row or nothing
      {
        static std::vector<std::string> buf;
        const auto&                     row = *row_opt;
        std::string                     key;
        if (row_opt.has_value()) {
          key = std::get<std::string>(row[0]);
          world.async(0, [](const auto& val) { buf.push_back(val); }, key);
        }
        world.barrier();
        YGM_ASSERT_RELEASE(buf.size() <= size_t(world.size()));

        std::unordered_set<std::string> unique_values(buf.begin(), buf.end());
        YGM_ASSERT_RELEASE(unique_values.size() == buf.size());
      }
    }
  }
  return 0;
}
