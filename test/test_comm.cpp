// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG
#include <ygm/comm.hpp>
#include <ygm/detail/ygm_ptr.hpp>

int main() {
  YGM_ASSERT_MPI(MPI_Init(nullptr, nullptr));

  std::vector<std::string> routing_schemes{"NONE", "NR", "NLNR"};
  for (const auto& routing_scheme : routing_schemes) {
    setenv("YGM_COMM_ROUTING", routing_scheme.c_str(), 1);

    ygm::comm world(MPI_COMM_WORLD);

    //
    // Test Rank 0 async to all others
    {
      size_t counter{};
      auto   pcounter = world.make_ygm_ptr(counter);
      if (world.rank0()) {
        for (int dest = 0; dest < world.size(); ++dest) {
          world.async(dest, [](auto pcounter) { (*pcounter)++; }, pcounter);
        }
      }
      world.barrier();
      YGM_ASSERT_RELEASE(counter == 1);
    }

    /*
    //
    // Test all ranks async to all others
    {
      size_t counter{};
      auto   pcounter = world.make_ygm_ptr(counter);
      for (int dest = 0; dest < world.size(); ++dest) {
        world.async(
            dest, [](auto pcounter) { (*pcounter)++; }, pcounter);
      }
      world.barrier();
      YGM_ASSERT_RELEASE(counter == (size_t)world.size());
    }

    //
    // Test async_bcast
    {
      size_t counter{};
      auto   pcounter = world.make_ygm_ptr(counter);
      if (world.rank0()) {
        world.async_bcast([](auto pcounter) { (*pcounter)++; }, pcounter);
      }

      world.barrier();
      YGM_ASSERT_RELEASE(counter == 1);
    }

    {
      size_t counter{};
      int    num_bcasts = 100;
      auto   pcounter   = world.make_ygm_ptr(counter);
      for (int i = 0; i < num_bcasts; ++i) {
        world.async_bcast([](auto pcounter) { (*pcounter)++; }, pcounter);
      }

      world.barrier();
      YGM_ASSERT_RELEASE(counter == num_bcasts * world.size());
    }

    //
    // Test async_mcast
    {
      size_t counter{};
      auto   pcounter = world.make_ygm_ptr(counter);
      if (world.rank0()) {
        std::vector<int> dests;
        for (int dest = 0; dest < world.size(); dest += 2) {
          dests.push_back(dest);
        }
        world.async_mcast(
            dests, [](auto pcounter) { (*pcounter)++; }, pcounter);
      }

      world.barrier();
      if (world.rank() % 2) {
        YGM_ASSERT_RELEASE(counter == 0);
      } else {
        YGM_ASSERT_RELEASE(counter == 1);
      }
    }

    //
    // Test reductions
    {
      auto max = ygm::max(size_t(world.rank()), world);
      YGM_ASSERT_RELEASE(max == (size_t)world.size() - 1);

      auto min = ygm::min(size_t(world.rank()), world);
      YGM_ASSERT_RELEASE(min == 0);

      auto sum = ygm::sum(size_t(world.rank()), world);
      YGM_ASSERT_RELEASE(sum ==
                     (((size_t)world.size() - 1) * (size_t)world.size()) / 2);

      size_t id  = world.rank();
      auto   red = ygm::all_reduce(id, [](size_t a, size_t b) {
        if (a < b) {
          return a;
        } else {
          return b;
        }
      }, world);
      YGM_ASSERT_RELEASE(red == 0);
      auto red2 = ygm::all_reduce(id, [](size_t a, size_t b) {
        if (a > b) {
          return a;
        } else {
          return b;
        }
      }, world);
      YGM_ASSERT_RELEASE(red2 == (size_t)world.size() - 1);
    }

    //
    // Test wait_until
    {
      static bool done = false;
      world.cf_barrier();
      world.async_bcast([]() { done = true; });
      world.local_wait_until([]() { return done; });
      world.barrier();
      YGM_ASSERT_RELEASE(done);
    }
    */
  }

  YGM_ASSERT_MPI(MPI_Finalize());
  return 0;
}
