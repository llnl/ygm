// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG

#include <filesystem>
#include <ygm/comm.hpp>
#include <ygm/io/line_parser.hpp>
#include <ygm/io/multi_output.hpp>

namespace fs = std::filesystem;

int main(int argc, char **argv) {
  ygm::comm world(&argc, &argv);

  // Check files created
  {
    std::string base_dir{"test_dir/"};
    std::string prefix_path{base_dir + std::string("nested_dir/")};

    std::string subpath("dir/out" + std::to_string(world.rank()));
    std::string message("my message from rank " + std::to_string(world.rank()));

    {
      ygm::io::multi_output mo(world, prefix_path, 1024, false);

      mo.async_write_line(subpath, message);
    }

    std::string expected_path(prefix_path + subpath);
    YGM_ASSERT_RELEASE(fs::exists(fs::path(expected_path)));

    world.barrier();

    if (world.rank0()) {
      fs::remove_all(fs::path(base_dir));
    }
  }

  // Test writing
  {
    uint64_t    xor_write;
    uint64_t    xor_read{0};
    std::string base_dir{"test_dir/"};
    std::string prefix_path{base_dir + std::string("nested_dir/")};

    // Write lines to file
    {
      ygm::io::multi_output mo(world, prefix_path, 1024, false);

      std::string subpath("dir/out" + std::to_string(world.rank()));
      std::string message("my message from rank " +
                          std::to_string(world.rank()));

      xor_write = std::hash<std::string>()(message);
      mo.async_write_line(subpath, message);

      xor_write = ygm::all_reduce(
          xor_write, [](const uint64_t a, const uint64_t b) { return a ^ b; },
          world);
    }

    // Read lines back
    {
      ygm::io::line_parser linep(world, std::vector<std::string>{prefix_path},
                                 false, true);

      linep.for_all([&xor_read](const std::string &line) {
        xor_read ^= std::hash<std::string>()(line);
      });

      xor_read = ygm::all_reduce(
          xor_read, [](const uint64_t a, const uint64_t b) { return a ^ b; },
          world);
    }

    // Clean up files
    if (world.rank0()) {
      fs::remove_all(fs::path(base_dir));
    }

    YGM_ASSERT_RELEASE(xor_write == xor_read);
  }

  // Test appending
  {
    uint64_t    xor_write;
    uint64_t    xor_read{0};
    std::string base_dir{"test_dir/"};
    std::string prefix_path{base_dir + std::string("nested_dir/")};

    // Write lines to file
    {
      ygm::io::multi_output mo(world, prefix_path, 1024, false);

      std::string subpath("dir/out" + std::to_string(world.rank()));
      std::string message("my message from rank " +
                          std::to_string(world.rank()));

      xor_write = std::hash<std::string>()(message);
      mo.async_write_line(subpath, message);
    }

    // Append new lines
    {
      ygm::io::multi_output mo(world, prefix_path, 1024, true);

      std::string subpath("dir/out" + std::to_string(world.rank() + 1));
      std::string message("my second message from rank " +
                          std::to_string(world.rank()));

      xor_write ^= std::hash<std::string>()(message);
      mo.async_write_line(subpath, message);

      xor_write = ygm::all_reduce(
          xor_write, [](const uint64_t a, const uint64_t b) { return a ^ b; },
          world);
    }

    // Read lines back
    {
      ygm::io::line_parser linep(world, std::vector<std::string>{prefix_path},
                                 false, true);

      linep.for_all([&xor_read](const std::string &line) {
        xor_read ^= std::hash<std::string>()(line);
      });

      xor_read = ygm::all_reduce(
          xor_read, [](const uint64_t a, const uint64_t b) { return a ^ b; },
          world);
    }

    // Clean up files
    if (world.rank0()) {
      fs::remove_all(fs::path(base_dir));
    }

    YGM_ASSERT_RELEASE(xor_write == xor_read);
  }

  return 0;
}
