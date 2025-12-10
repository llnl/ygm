// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG

#include <set>
#include <string>
#include <vector>
#include <ygm/comm.hpp>
#include <ygm/container/bag.hpp>
#include <ygm/container/counting_set.hpp>

int main(int argc, char** argv) {
  ygm::comm world(&argc, &argv);

  {
    ygm::container::bag<int> ibag(world, {42, 1, 8, 16, 32, 3, 4, 5, 6, 7});

    int sum = ibag.reduce(std::plus<int>());
    YGM_ASSERT_RELEASE(sum == 124);

    int even_sum =
        ibag.filter([](int i) { return i % 2 == 0; }).reduce(std::plus<int>());
    YGM_ASSERT_RELEASE(even_sum == 108);
  }
}
