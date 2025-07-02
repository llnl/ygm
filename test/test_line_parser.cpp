// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG

#include <filesystem>
#include <ygm/comm.hpp>
#include <ygm/container/counting_set.hpp>
#include <ygm/io/line_parser.hpp>

namespace fs = std::filesystem;

void test_line_parser_files(ygm::comm&, const std::vector<std::string>&);
void test_line_parser_directory(ygm::comm& , const std::string& , size_t );

int main(int argc, char** argv) {
  ygm::comm world(&argc, &argv);

  {
    test_line_parser_files(world, {"data/short.txt"});
    test_line_parser_files(world, {"data/loremipsum/loremipsum_0.txt"});
    test_line_parser_files(world, {"data/loremipsum/loremipsum_0.txt",
                             "data/loremipsum/loremipsum_1.txt"});
    test_line_parser_files(world, {"data/loremipsum/loremipsum_0.txt",
                             "data/loremipsum/loremipsum_1.txt",
                             "data/loremipsum/loremipsum_2.txt"});
    test_line_parser_files(world, {"data/loremipsum/loremipsum_0.txt",
                             "data/loremipsum/loremipsum_1.txt",
                             "data/loremipsum/loremipsum_2.txt",
                             "data/loremipsum/loremipsum_3.txt"});
    test_line_parser_files(
        world,
        {"data/loremipsum/loremipsum_0.txt", "data/loremipsum/loremipsum_1.txt",
         "data/loremipsum/loremipsum_2.txt", "data/loremipsum/loremipsum_3.txt",
         "data/loremipsum/loremipsum_4.txt"});
    test_line_parser_files(world, {"data/loremipsum_large.txt"});
    test_line_parser_files(
        world,
        {"data/loremipsum/loremipsum_0.txt", "data/loremipsum/loremipsum_1.txt",
         "data/loremipsum/loremipsum_2.txt", "data/loremipsum/loremipsum_3.txt",
         "data/loremipsum/loremipsum_4.txt", "data/loremipsum_large.txt"});

    test_line_parser_directory(world, "data/loremipsum", 270);
    test_line_parser_directory(world, "data/loremipsum/", 270);
  }

  return 0;
}

void test_line_parser_files(ygm::comm& comm, const std::vector<std::string>& files) {
  //
  // Read in each line into a distributed set
  ygm::container::counting_set<std::string> line_set_to_test(comm);
  ygm::io::line_parser                      bfr(comm, files);
  bfr.for_all([&line_set_to_test](const std::string& line) {
    line_set_to_test.async_insert(line);
  });

  //
  // Read each line sequentially
  ygm::container::counting_set<std::string> line_set(comm);
  std::set<std::string>                     line_set_sequential;
  for (const auto& f : files) {
    std::ifstream ifs(f.c_str());
    YGM_ASSERT_RELEASE(ifs.good());
    std::string line;
    while (std::getline(ifs, line)) {
      line_set.async_insert(line);
      line_set_sequential.insert(line);
    }
  }

  YGM_ASSERT_RELEASE(line_set.size() == line_set_sequential.size());
  //comm.cout0(line_set.size(), " =? ", line_set_to_test.size());
  YGM_ASSERT_RELEASE(line_set.size() == line_set_to_test.size());
  // YGM_ASSERT_RELEASE(line_set == line_set_to_test);
}


void test_line_parser_directory(ygm::comm& comm, const std::string& dir, size_t unique_line_count) {
  //
  // Read in each line into a distributed set
  ygm::container::counting_set<std::string> line_set_to_test(comm);
  ygm::io::line_parser                      bfr(comm, {dir});
  bfr.for_all([&line_set_to_test](const std::string& line) {
    line_set_to_test.async_insert(line);
  });

  YGM_ASSERT_RELEASE(unique_line_count == line_set_to_test.size());  
}