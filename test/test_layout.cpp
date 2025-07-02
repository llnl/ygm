// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG
#include <ygm/comm.hpp>
#include <ygm/detail/layout.hpp>

int main(int argc, char** argv) {
  ygm::comm world(&argc, &argv);

  //
  // node sizes agree
  {
    int node_size(world.layout().node_size());
    int min_node_size = ygm::min(node_size, world);
    world.barrier();
    YGM_ASSERT_RELEASE(min_node_size == node_size);
  }

  //
  // local sizes agree
  {
    int local_size(world.layout().local_size());
    int min_local_size = ygm::min(local_size, world);
    world.barrier();
    YGM_ASSERT_RELEASE(min_local_size == local_size);
  }

  //
  // local and node id computations agree locally
  {
    for (int dst(0); dst < world.size(); ++dst) {
      auto p = world.layout().rank_to_nl(dst);
      YGM_ASSERT_RELEASE(p.first == world.layout().node_id(dst));
      YGM_ASSERT_RELEASE(p.second == world.layout().local_id(dst));
    }
    world.barrier();
  }

  //
  // local and node id computations agree globally
  {
    if (world.rank0()) {
      for (int dst(0); dst < world.size(); ++dst) {
        auto check_fn = [](auto pcomm, int node_guess, int local_guess) {
          YGM_ASSERT_RELEASE(pcomm->layout().node_id() == node_guess);
          YGM_ASSERT_RELEASE(pcomm->layout().local_id() == local_guess);
        };
        auto p = world.layout().rank_to_nl(dst);
        world.async(dst, check_fn, p.first, p.second);
      }
    }
    world.barrier();
  }

  //
  // is_local() is correct
  {
    auto check_fn = [](auto pcomm, int rank, bool tru) {
      YGM_ASSERT_RELEASE(pcomm->layout().is_local(rank) == tru);
    };

    bool target = (world.layout().node_id() == 0) ? true : false;
    world.async(0, check_fn, world.layout().rank(), target);
    world.barrier();
  }

  //
  // cached strided ranks are correct
  {
    std::vector<int> strided_ranks = world.layout().strided_ranks();
    for (auto sr : strided_ranks) {
      YGM_ASSERT_RELEASE(world.layout().is_strided(sr) == true);
      if (world.layout().rank() != sr) {
        YGM_ASSERT_RELEASE(world.layout().is_local(sr) == false);
      }
    }
    world.barrier();
  }

  //
  // cached local ranks are correct
  {
    std::vector<int> local_ranks = world.layout().local_ranks();
    for (auto lr : local_ranks) {
      YGM_ASSERT_RELEASE(world.layout().is_local(lr) == true);
      if (world.layout().rank() != lr) {
        YGM_ASSERT_RELEASE(world.layout().is_strided(lr) == false);
      }
    }
    world.barrier();
  }

  {
    std::vector<int> strided_ranks = world.layout().strided_ranks();
    auto             check_fn      = [](auto pcomm, int src_rank) {
      YGM_ASSERT_RELEASE(pcomm->layout().is_strided(src_rank) == true);
      if (pcomm->layout().rank() != src_rank) {
        YGM_ASSERT_RELEASE(pcomm->layout().is_local(src_rank) == false);
      }
    };
    for (auto dst : strided_ranks) {
      world.async(dst, check_fn, world.layout().rank());
    }
    world.barrier();
  }

  {
    std::vector<int> local_ranks = world.layout().local_ranks();
    auto             check_fn    = [](auto pcomm, int src_rank) {
      YGM_ASSERT_RELEASE(pcomm->layout().is_local(src_rank) == true);
      if (pcomm->layout().rank() != src_rank) {
        YGM_ASSERT_RELEASE(pcomm->layout().is_strided(src_rank) == false);
      }
    };
    for (auto dst : local_ranks) {
      world.async(dst, check_fn, world.layout().rank());
    }
    world.barrier();
  }

  return 0;
}
