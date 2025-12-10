// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG
#include <random>
#include <vector>
#include <ygm/comm.hpp>

static std::default_random_engine gen;
static int                        max_hops = 15;

// Recursive message spawns multiple children to random destinations
struct recursive_functor {
  template <typename Comm>
  void operator()(Comm *pcomm, const std::vector<int> &vec, int hops,
                  ygm::ygm_ptr<size_t> pcounter) {
    (*pcounter)++;
    if (hops < max_hops) {
      for (int i = 0; i < 2; ++i) {
        std::uniform_int_distribution<int> dist(0, pcomm->size() - 1);
        int                                dest = dist(gen);
        pcomm->async(dest, recursive_functor(), vec, hops + 1, pcounter);
      }
    }
  }
};

int main(int argc, char **argv) {
  // Create comm for very small messages
  ::setenv("YGM_COMM_BUFFER_SIZE_KB", "1", 1);
  ygm::comm world(&argc, &argv);

  // Test large recursive doubling message from rank 0
  {
    size_t           vec_size = 1024;
    std::vector<int> my_vec(vec_size);

    for (size_t i = 0; i < my_vec.size(); ++i) {
      my_vec[i] = i;
    }

    size_t counter{0};
    auto   pcounter = world.make_ygm_ptr(counter);
    if (world.rank0()) {
      world.async(0, recursive_functor(), my_vec, 1, pcounter);
    }

    world.barrier();
    YGM_ASSERT_RELEASE(ygm::sum(counter, world) ==
                       ((size_t(1) << max_hops) - 1));
  }

  return 0;
}
