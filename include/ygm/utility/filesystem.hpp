// Copyright 2019-2026 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

namespace ygm::utility::fs {
/**
 * @brief Create all directories necessary to contain input path
 *
 * @param p Path to file
 */
inline void make_directories(const ::fs::path &p) {
  std::vector<::fs::path> directory_stack;
  ::fs::path              curr_path = p.parent_path();

  while (!::fs::exists(curr_path) && !curr_path.empty()) {
    directory_stack.push_back(curr_path);
    curr_path = curr_path.parent_path();
  }

  while (directory_stack.size() > 0) {
    ::fs::path &p = directory_stack.back();
    ::fs::create_directory(p);
    directory_stack.pop_back();
  }
}
}  // namespace ygm::utility::fs
