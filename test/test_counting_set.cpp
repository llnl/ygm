// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG
#include <string>

#include <ygm/comm.hpp>
#include <ygm/container/counting_set.hpp>

int main(int argc, char **argv) {
  ygm::comm world(&argc, &argv);

  // Test basic tagging
  {
    ygm::container::counting_set<std::string> cset(world);

    static_assert(std::is_same_v<decltype(cset)::self_type, decltype(cset)>);
    static_assert(std::is_same_v<decltype(cset)::mapped_type, size_t>);
    static_assert(std::is_same_v<decltype(cset)::key_type, std::string>);
    static_assert(std::is_same_v<decltype(cset)::size_type, size_t>);
    static_assert(
        std::is_same_v<decltype(cset)::for_all_args,
                       std::tuple<decltype(cset)::key_type, size_t> >);
  }

  //
  // Test Rank 0 async_insert
  {
    ygm::container::counting_set<std::string> cset(world);
    if (world.rank() == 0) {
      cset.async_insert("dog");
      cset.async_insert("apple");
      cset.async_insert("red");
    }

    YGM_ASSERT_RELEASE(cset.count("dog") == 1);
    YGM_ASSERT_RELEASE(cset.count("apple") == 1);
    YGM_ASSERT_RELEASE(cset.count("red") == 1);
    YGM_ASSERT_RELEASE(cset.size() == 3);

    auto count_map = cset.gather_keys({"dog", "cat", "apple"});
    YGM_ASSERT_RELEASE(count_map["dog"] == 1);
    YGM_ASSERT_RELEASE(count_map["apple"] == 1);
    YGM_ASSERT_RELEASE(count_map.count("cat") == 0);

    // test contains
    YGM_ASSERT_RELEASE(cset.contains("dog"));
    YGM_ASSERT_RELEASE(cset.contains("apple"));
    YGM_ASSERT_RELEASE(cset.contains("red"));
    YGM_ASSERT_RELEASE(!cset.contains("blue"));
  }

  //
  // Test all ranks async_insert
  {
    ygm::container::counting_set<std::string> cset(world);

    cset.async_insert("dog");
    cset.async_insert("apple");
    cset.async_insert("red");

    YGM_ASSERT_RELEASE(cset.count("dog") == (size_t)world.size());
    YGM_ASSERT_RELEASE(cset.count("apple") == (size_t)world.size());
    YGM_ASSERT_RELEASE(cset.count("red") == (size_t)world.size());
    YGM_ASSERT_RELEASE(cset.size() == 3);

    auto count_map = cset.gather_keys({"dog", "cat", "apple"});
    YGM_ASSERT_RELEASE(count_map["dog"] == (size_t)world.size());
    YGM_ASSERT_RELEASE(count_map["apple"] == (size_t)world.size());
    YGM_ASSERT_RELEASE(cset.count("cat") == 0);

    YGM_ASSERT_RELEASE(cset.count_all() == 3 * (size_t)world.size());

    // test contains
    YGM_ASSERT_RELEASE(cset.contains("dog"));
    YGM_ASSERT_RELEASE(cset.contains("apple"));
    YGM_ASSERT_RELEASE(cset.contains("red"));
    YGM_ASSERT_RELEASE(!cset.contains("blue"));
  }

  //
  // Test counting_sets YGM pointer
  {
    ygm::container::counting_set<std::string> cset(world);

    auto cset_ptr = cset.get_ygm_ptr();

    // Mix operations with pointer and counting_set
    cset_ptr->async_insert("dog");
    cset_ptr->async_insert("apple");
    cset.async_insert("red");

    YGM_ASSERT_RELEASE(cset_ptr->count("dog") == (size_t)world.size());
    YGM_ASSERT_RELEASE(cset_ptr->count("apple") == (size_t)world.size());
    YGM_ASSERT_RELEASE(cset.count("red") == (size_t)world.size());
    YGM_ASSERT_RELEASE(cset.size() == 3);

    auto count_map = cset.gather_keys({"dog", "cat", "apple"});
    YGM_ASSERT_RELEASE(count_map["dog"] == (size_t)world.size());
    YGM_ASSERT_RELEASE(count_map["apple"] == (size_t)world.size());
    YGM_ASSERT_RELEASE(cset.count("cat") == 0);

    YGM_ASSERT_RELEASE(cset.count_all() == 3 * (size_t)world.size());
  }

  //
  // Test clear
  {
    ygm::container::counting_set<std::string> cset(world);
    if (world.rank() == 0) {
      cset.async_insert("dog");
      cset.async_insert("apple");
      cset.async_insert("red");
    }

    YGM_ASSERT_RELEASE(cset.count("dog") == 1);
    YGM_ASSERT_RELEASE(cset.count("apple") == 1);
    YGM_ASSERT_RELEASE(cset.count("red") == 1);
    YGM_ASSERT_RELEASE(cset.size() == 3);

    cset.clear();
    YGM_ASSERT_RELEASE(cset.size() == 0);
    YGM_ASSERT_RELEASE(cset.count("dog") == 0);
    YGM_ASSERT_RELEASE(cset.count("apple") == 0);
    YGM_ASSERT_RELEASE(cset.count("red") == 0);

    // test contains
    YGM_ASSERT_RELEASE(!cset.contains("dog"));
    YGM_ASSERT_RELEASE(!cset.contains("apple"));
    YGM_ASSERT_RELEASE(!cset.contains("red"));
    YGM_ASSERT_RELEASE(!cset.contains("blue"));
  }

  // //
  // // Test topk
  // {
  //   ygm::container::counting_set<std::string> cset(world);

  //   cset.async_insert("dog");
  //   cset.async_insert("dog");
  //   cset.async_insert("dog");
  //   cset.async_insert("cat");
  //   cset.async_insert("cat");
  //   cset.async_insert("bird");

  //   auto topk = cset.topk(
  //       2, [](const auto &a, const auto &b) { return a.second > b.second; });

  //   YGM_ASSERT_RELEASE(topk[0].first == "dog");
  //   YGM_ASSERT_RELEASE(topk[0].second == 3 * world.size());
  //   YGM_ASSERT_RELEASE(topk[1].first == "cat");
  //   YGM_ASSERT_RELEASE(topk[1].second == 2 * world.size());
  // }

  //
  // Test for_all
  {
    ygm::container::counting_set<std::string> cset1(world);
    ygm::container::counting_set<std::string> cset2(world);

    cset1.async_insert("dog");
    cset1.async_insert("dog");
    cset1.async_insert("dog");
    cset1.async_insert("cat");
    cset1.async_insert("cat");
    cset1.async_insert("bird");

    YGM_ASSERT_RELEASE(cset1.count("dog") == (size_t)world.size() * 3);
    YGM_ASSERT_RELEASE(cset1.count("cat") == (size_t)world.size() * 2);
    YGM_ASSERT_RELEASE(cset1.count("bird") == (size_t)world.size());
    YGM_ASSERT_RELEASE(cset1.count("red") == 0);
    YGM_ASSERT_RELEASE(cset1.size() == 3);

    cset1.for_all([&cset2](const auto &key, const auto &value) {
      for (int i = 0; i < value; i++) {
        cset2.async_insert(key);
      }
    });

    YGM_ASSERT_RELEASE(cset2.count("dog") == (size_t)world.size() * 3);
    YGM_ASSERT_RELEASE(cset2.count("cat") == (size_t)world.size() * 2);
    YGM_ASSERT_RELEASE(cset2.count("bird") == (size_t)world.size());
    YGM_ASSERT_RELEASE(cset2.count("red") == 0);
    YGM_ASSERT_RELEASE(cset2.size() == 3);

    // test contains
    YGM_ASSERT_RELEASE(cset2.contains("dog"));
    YGM_ASSERT_RELEASE(cset2.contains("cat"));
    YGM_ASSERT_RELEASE(cset2.contains("bird"));
    YGM_ASSERT_RELEASE(!cset2.contains("red"));
  }

  //
  // Test copy constructor
  {
    ygm::container::counting_set<std::string> cset(world);
    if (world.rank() == 0) {
      cset.async_insert("dog");
      cset.async_insert("apple");
      cset.async_insert("red");
    }

    ygm::container::counting_set<std::string> cset2(cset);

    YGM_ASSERT_RELEASE(cset.count("dog") == 1);
    YGM_ASSERT_RELEASE(cset.count("apple") == 1);
    YGM_ASSERT_RELEASE(cset.count("red") == 1);
    YGM_ASSERT_RELEASE(cset.size() == 3);

    YGM_ASSERT_RELEASE(cset2.count("dog") == 1);
    YGM_ASSERT_RELEASE(cset2.count("apple") == 1);
    YGM_ASSERT_RELEASE(cset2.count("red") == 1);
    YGM_ASSERT_RELEASE(cset2.size() == 3);

    if (world.rank0()) {
      cset2.async_insert("dog");
      cset2.async_insert("apple");
      cset2.async_insert("red");
    }

    YGM_ASSERT_RELEASE(cset.count("dog") == 1);
    YGM_ASSERT_RELEASE(cset.count("apple") == 1);
    YGM_ASSERT_RELEASE(cset.count("red") == 1);
    YGM_ASSERT_RELEASE(cset.size() == 3);

    YGM_ASSERT_RELEASE(cset2.count("dog") == 2);
    YGM_ASSERT_RELEASE(cset2.count("apple") == 2);
    YGM_ASSERT_RELEASE(cset2.count("red") == 2);
    YGM_ASSERT_RELEASE(cset2.size() == 6);
  }

  //
  // Test copy assignment operator
  {
    ygm::container::counting_set<std::string> cset(world);
    if (world.rank() == 0) {
      cset.async_insert("dog");
      cset.async_insert("apple");
      cset.async_insert("red");
    }

    ygm::container::counting_set<std::string> cset2(world);
    cset2 = cset;

    YGM_ASSERT_RELEASE(cset.count("dog") == 1);
    YGM_ASSERT_RELEASE(cset.count("apple") == 1);
    YGM_ASSERT_RELEASE(cset.count("red") == 1);
    YGM_ASSERT_RELEASE(cset.size() == 3);

    YGM_ASSERT_RELEASE(cset2.count("dog") == 1);
    YGM_ASSERT_RELEASE(cset2.count("apple") == 1);
    YGM_ASSERT_RELEASE(cset2.count("red") == 1);
    YGM_ASSERT_RELEASE(cset2.size() == 3);

    if (world.rank0()) {
      cset2.async_insert("dog");
      cset2.async_insert("apple");
      cset2.async_insert("red");
    }

    YGM_ASSERT_RELEASE(cset.count("dog") == 1);
    YGM_ASSERT_RELEASE(cset.count("apple") == 1);
    YGM_ASSERT_RELEASE(cset.count("red") == 1);
    YGM_ASSERT_RELEASE(cset.size() == 3);

    YGM_ASSERT_RELEASE(cset2.count("dog") == 2);
    YGM_ASSERT_RELEASE(cset2.count("apple") == 2);
    YGM_ASSERT_RELEASE(cset2.count("red") == 2);
    YGM_ASSERT_RELEASE(cset2.size() == 6);
  }

  //
  // Test move constructor
  {
    ygm::container::counting_set<std::string> cset(world);
    if (world.rank() == 0) {
      cset.async_insert("dog");
      cset.async_insert("apple");
      cset.async_insert("red");
    }

    ygm::container::counting_set<std::string> cset2(std::move(cset));

    YGM_ASSERT_RELEASE(cset.count("dog") == 0);
    YGM_ASSERT_RELEASE(cset.count("apple") == 0);
    YGM_ASSERT_RELEASE(cset.count("red") == 0);
    YGM_ASSERT_RELEASE(cset.size() == 0);

    YGM_ASSERT_RELEASE(cset2.count("dog") == 1);
    YGM_ASSERT_RELEASE(cset2.count("apple") == 1);
    YGM_ASSERT_RELEASE(cset2.count("red") == 1);
    YGM_ASSERT_RELEASE(cset2.size() == 3);

    if (world.rank0()) {
      cset2.async_insert("dog");
      cset2.async_insert("apple");
      cset2.async_insert("red");
    }

    YGM_ASSERT_RELEASE(cset.count("dog") == 0);
    YGM_ASSERT_RELEASE(cset.count("apple") == 0);
    YGM_ASSERT_RELEASE(cset.count("red") == 0);
    YGM_ASSERT_RELEASE(cset.size() == 0);

    YGM_ASSERT_RELEASE(cset2.count("dog") == 2);
    YGM_ASSERT_RELEASE(cset2.count("apple") == 2);
    YGM_ASSERT_RELEASE(cset2.count("red") == 2);
    YGM_ASSERT_RELEASE(cset2.size() == 6);
  }

  //
  // Test move assignment operator
  {
    ygm::container::counting_set<std::string> cset(world);
    if (world.rank() == 0) {
      cset.async_insert("dog");
      cset.async_insert("apple");
      cset.async_insert("red");
    }

    ygm::container::counting_set<std::string> cset2(world);
    cset2 = std::move(cset);

    YGM_ASSERT_RELEASE(cset.count("dog") == 0);
    YGM_ASSERT_RELEASE(cset.count("apple") == 0);
    YGM_ASSERT_RELEASE(cset.count("red") == 0);
    YGM_ASSERT_RELEASE(cset.size() == 0);

    YGM_ASSERT_RELEASE(cset2.count("dog") == 1);
    YGM_ASSERT_RELEASE(cset2.count("apple") == 1);
    YGM_ASSERT_RELEASE(cset2.count("red") == 1);
    YGM_ASSERT_RELEASE(cset2.size() == 3);

    if (world.rank0()) {
      cset2.async_insert("dog");
      cset2.async_insert("apple");
      cset2.async_insert("red");
    }

    YGM_ASSERT_RELEASE(cset.count("dog") == 0);
    YGM_ASSERT_RELEASE(cset.count("apple") == 0);
    YGM_ASSERT_RELEASE(cset.count("red") == 0);
    YGM_ASSERT_RELEASE(cset.size() == 0);

    YGM_ASSERT_RELEASE(cset2.count("dog") == 2);
    YGM_ASSERT_RELEASE(cset2.count("apple") == 2);
    YGM_ASSERT_RELEASE(cset2.count("red") == 2);
    YGM_ASSERT_RELEASE(cset2.size() == 6);
  }

  return 0;
}
