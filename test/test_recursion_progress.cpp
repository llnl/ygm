// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG
#include <ygm/comm.hpp>

static size_t counter = 0;

// Recursive message spawns one child to next rank.
struct recursive_functor {
  template <typename Comm>
  void operator()(Comm *pcomm, size_t hops) {
    counter++;

    hops--;

    if (hops > 0) {
      pcomm->async((pcomm->rank() + 1) % pcomm->size(), recursive_functor(),
                   hops);
      pcomm->local_progress();
    }
  }
};

int main(int argc, char **argv) {
  ygm::comm world(&argc, &argv);

  // Test recursion with local_progress() in 'around the world' format
  {
    size_t trips = 100;

    size_t desired_hops = world.size() * trips + 1;

    if (world.rank0()) {
      world.async(0, recursive_functor(), desired_hops);
    }

    world.barrier();
    YGM_ASSERT_RELEASE(ygm::sum(counter, world) == (world.size() * trips + 1));
  }

  return 0;
}
