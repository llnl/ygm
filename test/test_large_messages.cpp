// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG
#include <cstdlib>
#include <ygm/comm.hpp>
#include <ygm/detail/ygm_ptr.hpp>

int main(int argc, char** argv) {
  // Create comm for very small messages
  ::setenv("YGM_COMM_BUFFER_SIZE_KB", "1", 1);
  ygm::comm world(&argc, &argv);

  // Test Rank 0 large message to all ranks
  {
    size_t large_msg_size = 1024 * 1024;
    size_t counter{};
    auto   pcounter = world.make_ygm_ptr(counter);
    if (world.rank() == 0) {
      std::vector<size_t> large_msg(large_msg_size);
      for (int dest = 0; dest < world.size(); ++dest) {
        // Count elements in large message's vector
        world.async(
            dest,
            [](auto pcounter, const std::vector<size_t>& vec) {
              for (size_t i = 0; i < vec.size(); ++i) {
                (*pcounter)++;
              }
            },
            pcounter, large_msg);
      }
    }

    world.barrier();
    YGM_ASSERT_RELEASE(counter == large_msg_size);
  }

  return 0;
}
