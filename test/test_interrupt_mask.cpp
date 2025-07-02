// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG
#include <ygm/comm.hpp>
#include <ygm/detail/interrupt_mask.hpp>

int main(int argc, char** argv) {
  ::setenv("YGM_COMM_BUFFER_SIZE_KB", "1", 1);
  ygm::comm world(&argc, &argv);

  int  count{0};
  auto count_ptr = world.make_ygm_ptr(count);
  int  num_sends{100};
  {
    ygm::detail::interrupt_mask mask(world);

    for (int i = 0; i < num_sends; ++i) {
      world.async(
          0, [](auto count_ptr) { ++(*count_ptr); }, count_ptr);
    }

    world.cf_barrier();

    YGM_ASSERT_RELEASE(count == 0);
  }

  world.barrier();

  if (world.rank0()) {
    YGM_ASSERT_RELEASE(count == num_sends * world.size());
  }

  return 0;
}
