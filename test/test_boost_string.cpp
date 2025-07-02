// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG

#include <ygm/comm.hpp>
#include <ygm/container/bag.hpp>
#include <ygm/container/set.hpp>
#include <ygm/utility/boost_static_string.hpp>

int main(int argc, char **argv) {
  ygm::comm world(&argc, &argv);

  {
    ygm::container::set<boost::static_string<16>> sset(world);
    sset.async_insert("dog");
    sset.async_insert("apple");
    sset.async_insert("red");
    world.barrier();
    for (auto s : sset) {
      world.cout(s);
    }
  }
}