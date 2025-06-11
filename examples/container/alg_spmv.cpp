// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#include <iostream>

#include <string>
#include <vector>
#include <ygm/comm.hpp>
#include <ygm/container/experimental/maptrix.hpp>
#include <ygm/container/map.hpp>
#include <ygm/io/line_parser.hpp>
#include <ygm/utility.hpp>

int main(int argc, char **argv) {
  ygm::comm world(&argc, &argv);

  using map_type     = ygm::container::map<size_t, int>;
  using maptrix_type = ygm::container::experimental::maptrix<size_t, int>;

  namespace ns_spmv = ygm::container::experimental::detail::algorithms;

  if (argc == 1) {
    std::cout << "Expected parameter arguments, exiting.." << std::endl;
    exit(0);
  }

  std::vector<std::string> mat_files({argv[1]});
  std::vector<std::string> vec_files;
  bool                     read_vec{false};
  if (argc == 3) {
    vec_files.push_back(argv[2]);
    read_vec = true;
  }

  map_type     x(world);
  maptrix_type A(world);

  world.cout0("Reading maptrix");
  ygm::io::line_parser line_parser(world, mat_files);
  line_parser.for_all([&A, &x, read_vec](auto &line) {
    size_t             src;
    size_t             dst;
    int                val = 1;
    std::istringstream iss(line);
    iss >> src >> dst >> val;
    A.async_insert(src, dst, val);

    /* Map x. */
    if (!read_vec) {
      x.async_insert(src, 1);
      x.async_insert(dst, 1);
    }
  });

  world.barrier();

  if (read_vec) {
    world.cout0("Reading vector");
    ygm::io::line_parser vec_parser(world, vec_files);
    vec_parser.for_all([&x](auto line) {
      size_t             index;
      size_t             val;
      std::istringstream iss(line);
      if (iss >> index >> val) {
        x.async_insert(index, val);
      }
    });
    world.barrier();
  }

  world.cout0("Performing SpMV");
  ygm::timer spmv_timer{};

  auto y = ns_spmv::spmv(A, x, std::plus<int>(), std::multiplies<int>());

  world.barrier();
  world.cout0("SpMV time: ", spmv_timer.elapsed(), " seconds");

  return 0;
}
