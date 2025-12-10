// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG

#include <string>
#include <ygm/comm.hpp>
#include <ygm/container/disjoint_set.hpp>

int main(int argc, char** argv) {
  ygm::comm world(&argc, &argv);

  // Test basic tagging
  {
    ygm::container::disjoint_set<std::string> dset(world);

    static_assert(std::is_same_v<decltype(dset)::self_type, decltype(dset)>);
    static_assert(std::is_same_v<decltype(dset)::value_type, std::string>);
    static_assert(std::is_same_v<decltype(dset)::size_type, size_t>);
    static_assert(std::is_same_v<decltype(dset)::ygm_for_all_types,
                                 std::tuple<decltype(dset)::value_type,
                                            decltype(dset)::value_type> >);
  }

  //
  // Test async_union from single rank
  {
    ygm::container::disjoint_set<std::string> dset(world);

    if (world.rank0()) {
      dset.async_union("cat", "cat");
      dset.async_union("dog", "dog");
      dset.async_union("car", "car");
    }

    world.barrier();

    if (world.rank0()) {
      dset.async_union("cat", "dog");
    }
    world.barrier();

    std::vector<std::string> to_find = {"cat", "dog", "car"};

    auto reps = dset.all_find(to_find);
    YGM_ASSERT_RELEASE(reps["cat"] == reps["dog"]);
    YGM_ASSERT_RELEASE(reps["cat"] != reps["car"]);
  }

  //
  // Test async_union from all ranks
  {
    ygm::container::disjoint_set<std::string> dset(world);

    if (world.rank0()) {
      dset.async_union("cat", "cat");
      dset.async_union("dog", "dog");
      dset.async_union("car", "car");
    }

    world.barrier();

    dset.async_union("cat", "dog");

    std::vector<std::string> to_find = {"cat", "dog", "car"};

    auto reps = dset.all_find(to_find);
    YGM_ASSERT_RELEASE(reps["cat"] == reps["dog"]);
    YGM_ASSERT_RELEASE(reps["cat"] != reps["car"]);
  }

  //
  // Test clear
  {
    ygm::container::disjoint_set<std::string> dset(world);

    if (world.rank0()) {
      dset.async_union("cat", "cat");
      dset.async_union("dog", "dog");
      dset.async_union("car", "car");
    }

    world.barrier();

    dset.async_union("cat", "dog");

    YGM_ASSERT_RELEASE(dset.size() == 3);
    YGM_ASSERT_RELEASE(dset.num_sets() == 2);

    dset.clear();

    YGM_ASSERT_RELEASE(dset.size() == 0);
    YGM_ASSERT_RELEASE(dset.num_sets() == 0);
  }

  //
  // Test longer union chains
  {
    ygm::container::disjoint_set<int> dset(world);

    if (world.rank0()) {
      dset.async_union(0, 0);
      dset.async_union(1, 1);
      dset.async_union(2, 2);
      dset.async_union(3, 3);
      dset.async_union(4, 4);
      dset.async_union(5, 5);
    }

    world.barrier();
    YGM_ASSERT_RELEASE(dset.num_sets() == 6);

    std::vector<int> to_find = {0, 1, 2, 3, 4, 5};

    dset.async_union(0, 1);
    dset.async_union(1, 2);

    dset.async_union(3, 4);
    dset.async_union(4, 5);

    YGM_ASSERT_RELEASE(dset.num_sets() == 2);

    auto reps = dset.all_find(to_find);
    YGM_ASSERT_RELEASE(reps[0] == reps[1]);
    YGM_ASSERT_RELEASE(reps[1] == reps[2]);
    YGM_ASSERT_RELEASE(reps[2] != reps[3]);
    YGM_ASSERT_RELEASE(reps[3] == reps[4]);
    YGM_ASSERT_RELEASE(reps[4] == reps[5]);

    dset.async_union(0, 3);
    YGM_ASSERT_RELEASE(dset.num_sets() == 1);

    auto reps_final = dset.all_find(to_find);
    YGM_ASSERT_RELEASE(reps_final[0] == reps_final[1]);
    YGM_ASSERT_RELEASE(reps_final[1] == reps_final[2]);
    YGM_ASSERT_RELEASE(reps_final[2] == reps_final[3]);
    YGM_ASSERT_RELEASE(reps_final[3] == reps_final[4]);
    YGM_ASSERT_RELEASE(reps_final[4] == reps_final[5]);
  }

  //
  // Test longer union chains with different union order
  {
    ygm::container::disjoint_set<int> dset(world);

    if (world.rank0()) {
      dset.async_union(0, 0);
      dset.async_union(1, 1);
      dset.async_union(2, 2);
      dset.async_union(3, 3);
      dset.async_union(4, 4);
      dset.async_union(5, 5);
    }

    world.barrier();
    YGM_ASSERT_RELEASE(dset.num_sets() == 6);

    std::vector<int> to_find = {0, 1, 2, 3, 4, 5};

    dset.async_union(0, 2);
    dset.async_union(1, 2);

    dset.async_union(4, 5);
    dset.async_union(3, 5);

    YGM_ASSERT_RELEASE(dset.num_sets() == 2);

    auto reps = dset.all_find(to_find);
    YGM_ASSERT_RELEASE(reps[0] == reps[1]);
    YGM_ASSERT_RELEASE(reps[1] == reps[2]);
    YGM_ASSERT_RELEASE(reps[2] != reps[3]);
    YGM_ASSERT_RELEASE(reps[3] == reps[4]);
    YGM_ASSERT_RELEASE(reps[4] == reps[5]);

    dset.async_union(0, 3);
    YGM_ASSERT_RELEASE(dset.num_sets() == 1);

    dset.all_compress();

    auto reps_final = dset.all_find(to_find);
    YGM_ASSERT_RELEASE(reps_final[0] == reps_final[1]);
    YGM_ASSERT_RELEASE(reps_final[1] == reps_final[2]);
    YGM_ASSERT_RELEASE(reps_final[2] == reps_final[3]);
    YGM_ASSERT_RELEASE(reps_final[3] == reps_final[4]);
    YGM_ASSERT_RELEASE(reps_final[4] == reps_final[5]);
  }

  //
  // Test for_all
  {
    ygm::container::disjoint_set<int> dset(world);
    int                               num_items = 4;

    int counter{0};

    for (int i = 0; i < num_items; ++i) {
      dset.async_union(i, i);
    }

    dset.for_all([&counter](const auto& item, const auto& rep) {
      YGM_ASSERT_RELEASE(item == rep);
      ++counter;
    });

    YGM_ASSERT_RELEASE(ygm::sum(counter, world) == num_items);
  }

  // Test async_union_and_execute
  {
    ygm::container::disjoint_set<int> dset(world);

    static int successful_counter{0};
    static int unsuccessful_counter{0};

    dset.async_union_and_execute(
        0, 1,
        []([[maybe_unused]] const int u, [[maybe_unused]] const int v,
           const bool union_result) {
          if (union_result) {
            successful_counter++;
          } else {
            unsuccessful_counter++;
          }
        });
    dset.async_union_and_execute(
        0, 2,
        []([[maybe_unused]] const int u, [[maybe_unused]] const int v,
           const bool union_result) {
          if (union_result) {
            successful_counter++;
          } else {
            unsuccessful_counter++;
          }
        });
    dset.async_union_and_execute(
        1, 2,
        []([[maybe_unused]] const int u, [[maybe_unused]] const int v,
           const bool union_result) {
          if (union_result) {
            successful_counter++;
          } else {
            unsuccessful_counter++;
          }
        });
    dset.async_union_and_execute(
        3, 4,
        []([[maybe_unused]] const int u, [[maybe_unused]] const int v,
           const bool union_result, [[maybe_unused]] const auto thing) {
          if (union_result) {
            successful_counter++;
          } else {
            unsuccessful_counter++;
          }
        },
        0);

    world.barrier();

    YGM_ASSERT_RELEASE(ygm::sum(successful_counter, world) == 3);
    YGM_ASSERT_RELEASE(ygm::sum(unsuccessful_counter, world) ==
                       world.size() * 4 - 3);
  }
}
