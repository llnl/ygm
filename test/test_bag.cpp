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
#include <ygm/random/random.hpp>

int main(int argc, char** argv) {
  ygm::comm world(&argc, &argv);

  // Test basic tagging
  {
    ygm::container::bag<std::string> bbag(world);

    static_assert(std::is_same_v<decltype(bbag)::self_type, decltype(bbag)>);
    static_assert(std::is_same_v<decltype(bbag)::value_type, std::string>);
    static_assert(std::is_same_v<decltype(bbag)::size_type, size_t>);
    static_assert(std::is_same_v<decltype(bbag)::for_all_args,
                                 std::tuple<decltype(bbag)::value_type>>);
  }

  //
  // Test Rank 0 async_insert
  {
    ygm::container::bag<std::string> bbag(world);
    if (world.rank0()) {
      bbag.async_insert("dog");
      bbag.async_insert("apple");
      bbag.async_insert("red");
    }
    YGM_ASSERT_RELEASE(bbag.count("dog") == 1);
    YGM_ASSERT_RELEASE(bbag.count("apple") == 1);
    YGM_ASSERT_RELEASE(bbag.count("red") == 1);
    YGM_ASSERT_RELEASE(bbag.size() == 3);

    // test contains.
    YGM_ASSERT_RELEASE(bbag.contains("dog"));
    YGM_ASSERT_RELEASE(bbag.contains("apple"));
    YGM_ASSERT_RELEASE(bbag.contains("red"));
    YGM_ASSERT_RELEASE(!bbag.contains("blue"));

    for (auto& value : bbag) {
      world.cout(value);
    }
  }

  //
  // Test copy constructor
  {
    ygm::container::bag<std::string> bbag(world);
    if (world.rank0()) {
      bbag.async_insert("dog");
      bbag.async_insert("apple");
      bbag.async_insert("red");
    }
    world.barrier();
    YGM_ASSERT_RELEASE(bbag.size() == 3);
    ygm::container::bag<std::string> bbag2(bbag);

    YGM_ASSERT_RELEASE(bbag.size() == 3);
    YGM_ASSERT_RELEASE(bbag2.size() == 3);

    if (world.rank0()) {
      bbag2.async_insert("car");
    }
    world.barrier();
    YGM_ASSERT_RELEASE(bbag.size() == 3);
    YGM_ASSERT_RELEASE(bbag2.size() == 4);
  }

  //
  // Test copy assignment operator
  {
    ygm::container::bag<std::string> bbag(world);
    if (world.rank0()) {
      bbag.async_insert("dog");
      bbag.async_insert("apple");
      bbag.async_insert("red");
    }
    world.barrier();
    YGM_ASSERT_RELEASE(bbag.size() == 3);
    ygm::container::bag<std::string> bbag2(world);
    bbag2 = bbag;

    YGM_ASSERT_RELEASE(bbag.size() == 3);
    YGM_ASSERT_RELEASE(bbag2.size() == 3);

    if (world.rank0()) {
      bbag2.async_insert("car");
    }
    world.barrier();
    YGM_ASSERT_RELEASE(bbag.size() == 3);
    YGM_ASSERT_RELEASE(bbag2.size() == 4);
  }

  //
  // Test move constructor
  {
    ygm::container::bag<std::string> bbag(world);
    if (world.rank0()) {
      bbag.async_insert("dog");
      bbag.async_insert("apple");
      bbag.async_insert("red");
    }
    world.barrier();
    YGM_ASSERT_RELEASE(bbag.size() == 3);
    ygm::container::bag<std::string> bbag2(std::move(bbag));

    YGM_ASSERT_RELEASE(bbag.size() == 0);
    YGM_ASSERT_RELEASE(bbag2.size() == 3);

    if (world.rank0()) {
      bbag2.async_insert("car");
    }
    world.barrier();
    YGM_ASSERT_RELEASE(bbag.size() == 0);
    YGM_ASSERT_RELEASE(bbag2.size() == 4);
  }

  // Testing = operator
  {
    ygm::container::bag<std::string> bbag(world);
    if (world.rank0()) {
      bbag.async_insert("dog");
      bbag.async_insert("apple");
      bbag.async_insert("red");
    }
    world.barrier();
    YGM_ASSERT_RELEASE(bbag.size() == 3);

    // ygm::container::bag<std::string> bbag2 = bbag;

    // YGM_ASSERT_RELEASE(bbag.size() == 3);
    // YGM_ASSERT_RELEASE(bbag2.size() == 3);

    // if (world.rank0()) {
    //   bbag2.async_insert("car");
    // }
    // world.barrier();
    // YGM_ASSERT_RELEASE(bbag.size() == 3);
    // YGM_ASSERT_RELEASE(bbag2.size() == 4);

    ygm::container::bag<std::string> bbag3 = std::move(bbag);
    YGM_ASSERT_RELEASE(bbag.size() == 0);
    YGM_ASSERT_RELEASE(bbag3.size() == 3);
  }

  //
  // Test all ranks async_insert
  {
    ygm::container::bag<std::string> bbag(world);
    bbag.async_insert("dog");
    bbag.async_insert("apple");
    bbag.async_insert("red");
    YGM_ASSERT_RELEASE(bbag.size() == 3 * (size_t)world.size());
    YGM_ASSERT_RELEASE(bbag.count("dog") == (size_t)world.size());
    YGM_ASSERT_RELEASE(bbag.count("apple") == (size_t)world.size());
    YGM_ASSERT_RELEASE(bbag.count("red") == (size_t)world.size());

    // test contains
    YGM_ASSERT_RELEASE(bbag.contains("dog"));
    YGM_ASSERT_RELEASE(bbag.contains("apple"));
    YGM_ASSERT_RELEASE(bbag.contains("red"));
    YGM_ASSERT_RELEASE(!bbag.contains("blue"));

    {
      std::vector<std::string> all_data;
      bbag.gather(all_data, 0);
      if (world.rank0()) {
        YGM_ASSERT_RELEASE(all_data.size() == 3 * (size_t)world.size());
      }
    }
    {
      std::set<std::string> all_data;
      bbag.gather(all_data, 0);
      if (world.rank0()) {
        YGM_ASSERT_RELEASE(all_data.size() == 3);
      }
    }
    {
      std::vector<std::string> all_data;
      bbag.gather(all_data);
      YGM_ASSERT_RELEASE(all_data.size() == 3 * (size_t)world.size());
    }
    {
      std::set<std::string> all_data;
      bbag.gather(all_data);
      YGM_ASSERT_RELEASE(all_data.size() == 3);
    }
  }

  //
  // Test reduce
  {
    ygm::container::bag<int> bbag(world);
    bbag.async_insert(1);
    bbag.async_insert(2);
    bbag.async_insert(3);
    YGM_ASSERT_RELEASE(bbag.reduce(std::plus<int>()) == 6 * world.size());
  }

  //
  // Test local_shuffle and global_shuffle
  {
    ygm::container::bag<int> bbag(world);
    int                      num_of_items = 20;
    if (world.rank0()) {
      for (int i = 0; i < num_of_items; i++) {
        bbag.async_insert(i);
      }
    }
    int                          seed = 100;
    ygm::random::default_random_engine<> rng1 =
        ygm::random::default_random_engine<>(world, seed);
    bbag.local_shuffle(rng1);

    ygm::random::default_random_engine<> rng2 =
        ygm::random::default_random_engine<>(world, seed);
    bbag.global_shuffle(rng2);

    bbag.local_shuffle();
    bbag.global_shuffle();

    YGM_ASSERT_RELEASE(bbag.size() == size_t(num_of_items));

    std::vector<int> bag_content;
    bbag.gather(bag_content, 0);
    if (world.rank0()) {
      for (int i = 0; i < num_of_items; i++) {
        if (std::find(bag_content.begin(), bag_content.end(), i) ==
            bag_content.end()) {
          YGM_ASSERT_RELEASE(false);
        }
      }
    }
  }

  //
  // Test for_all
  {
    ygm::container::bag<std::string> bbag(world);
    if (world.rank0()) {
      bbag.async_insert("dog");
      bbag.async_insert("apple");
      bbag.async_insert("red");
    }
    int count{0};
    bbag.for_all([&count]([[maybe_unused]] std::string& mstr) { ++count; });
    int global_count = ygm::sum(count, world);
    world.barrier();
    YGM_ASSERT_RELEASE(global_count == 3);
  }

  //
  // Test for_all (pair)
  {
    ygm::container::bag<std::pair<std::string, int>> pbag(world);
    if (world.rank0()) {
      pbag.async_insert({"dog", 1});
      pbag.async_insert({"apple", 2});
      pbag.async_insert({"red", 3});
    }
    int count{0};
    pbag.for_all(
        [&count](std::pair<std::string, int>& mstr) { count += mstr.second; });
    int global_count = ygm::sum(count, world);
    world.barrier();
    YGM_ASSERT_RELEASE(global_count == 6);
  }

  // //
  // // Test for_all (split pair)
  // {
  //   ygm::container::bag<std::pair<std::string, int>> pbag(world);
  //   if (world.rank0()) {
  //     pbag.async_insert({"dog", 1});
  //     pbag.async_insert({"apple", 2});
  //     pbag.async_insert({"red", 3});
  //   }
  //   int count{0};
  //   pbag.for_all(
  //       [&count](std::string& first, int& second) { count += second; });
  //   int global_count = ygm::sum(count, world);
  //   world.barrier();
  //   YGM_ASSERT_RELEASE(global_count == 6);
  // }

  //
  // Test rebalance
  {
    ygm::container::bag<std::string> bbag(world);
    bbag.async_insert("begin", 0);
    bbag.async_insert("end", world.size() - 1);
    bbag.rebalance();
    YGM_ASSERT_RELEASE(bbag.local_size() == 2);
  }

  //
  // Test rebalance with non-standard rebalance sizes
  {
    ygm::container::bag<std::string> bbag(world);
    bbag.async_insert("middle", world.size() / 2);
    bbag.async_insert("end", world.size() - 1);
    if (world.rank0()) bbag.async_insert("middle", world.size() / 2);
    bbag.rebalance();

    size_t remainder        = bbag.size() % world.size();
    size_t small_block_size = bbag.size() / world.size();
    size_t large_block_size =
        bbag.size() / world.size() + (bbag.size() % world.size() > 0);

    if (size_t(world.rank()) < remainder) {
      YGM_ASSERT_RELEASE(bbag.local_size() == large_block_size);
    } else {
      YGM_ASSERT_RELEASE(bbag.local_size() == small_block_size);
    }
  }

  //
  // Test output data after rebalance
  {
    ygm::container::bag<int> bbag(world);
    if (world.rank0()) {
      for (int i = 0; i < 100; i++) {
        bbag.async_insert(i, (i * 3) % world.size());
      }
      for (int i = 100; i < 200; i++) {
        bbag.async_insert(i, (i * 5) % world.size());
      }
    }
    bbag.rebalance();

    std::set<int> value_set;
    bbag.gather(value_set, 0);
    if (world.rank0()) {
      YGM_ASSERT_RELEASE(value_set.size() == 200);
      YGM_ASSERT_RELEASE(
          *std::min_element(value_set.begin(), value_set.end()) == 0);
      YGM_ASSERT_RELEASE(
          *std::max_element(value_set.begin(), value_set.end()) == 199);
    }
  }

  //
  // Test swap
  {
    ygm::container::bag<std::string> bbag(world);
    {
      ygm::container::bag<std::string> bbag2(world);
      if (world.rank0()) {
        bbag2.async_insert("dog");
        bbag2.async_insert("apple");
        bbag2.async_insert("red");
      }
      YGM_ASSERT_RELEASE(bbag2.size() == 3);
      bbag2.swap(bbag);
      YGM_ASSERT_RELEASE(bbag2.size() == 0);
    }
    YGM_ASSERT_RELEASE(bbag.size() == 3);
    YGM_ASSERT_RELEASE(bbag.count("dog") == 1);
    YGM_ASSERT_RELEASE(bbag.count("apple") == 1);
    YGM_ASSERT_RELEASE(bbag.count("red") == 1);
    if (world.rank0()) {
      bbag.async_insert("car");
    }
    YGM_ASSERT_RELEASE(bbag.size() == 4);
    YGM_ASSERT_RELEASE(bbag.count("car") == 1);
  }

  //
  // Test vector of bags
  {
    int                                   num_bags = 4;
    std::vector<ygm::container::bag<int>> vec_bags;

    for (int i = 0; i < num_bags; ++i) {
      vec_bags.emplace_back(world);
    }

    for (int bag_index = 0; bag_index < num_bags; ++bag_index) {
      int item = world.rank() + bag_index;
      vec_bags[bag_index].async_insert(item);
      vec_bags[bag_index].async_insert(item + 1);
    }

    world.barrier();
    for (int bag_index = 0; bag_index < num_bags; ++bag_index) {
      YGM_ASSERT_RELEASE(vec_bags[bag_index].size() ==
                         size_t(world.size() * 2));
    }
  }
}
