// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG

#include <string>
#include <ygm/comm.hpp>
#include <ygm/container/bag.hpp>
#include <ygm/container/set.hpp>

int main(int argc, char** argv) {
  ygm::comm world(&argc, &argv);

  //
  // Test basic tagging
  {
    ygm::container::set<std::string> sset(world);

    static_assert(std::is_same_v<decltype(sset)::self_type, decltype(sset)>);
    static_assert(std::is_same_v<decltype(sset)::value_type, std::string>);
    static_assert(std::is_same_v<decltype(sset)::size_type, size_t>);
    static_assert(std::is_same_v<decltype(sset)::for_all_args,
                                 std::tuple<decltype(sset)::value_type>>);
  }

  //
  // Test Rank 0 async_insert
  {
    ygm::container::set<std::string> sset(world);
    if (world.rank() == 0) {
      sset.async_insert("dog");
      sset.async_insert("apple");
      sset.async_insert("red");
    }
    YGM_ASSERT_RELEASE(sset.count("dog") == 1);
    YGM_ASSERT_RELEASE(sset.count("red") == 1);
    YGM_ASSERT_RELEASE(sset.count("apple") == 1);
    YGM_ASSERT_RELEASE(sset.size() == 3);

    // test contains.
    YGM_ASSERT_RELEASE(sset.contains("dog"));
    YGM_ASSERT_RELEASE(sset.contains("apple"));
    YGM_ASSERT_RELEASE(sset.contains("red"));
    YGM_ASSERT_RELEASE(!sset.contains("blue"));

    ygm::container::set<int> iset(world);
    if (world.rank() == 0) {
      iset.async_insert(42);
      iset.async_insert(7);
      iset.async_insert(100);
    }
    YGM_ASSERT_RELEASE(iset.count(42) == 1);
    YGM_ASSERT_RELEASE(iset.count(7) == 1);
    YGM_ASSERT_RELEASE(iset.count(100) == 1);
    YGM_ASSERT_RELEASE(iset.size() == 3);

    // test contains.
    YGM_ASSERT_RELEASE(iset.contains(42));
    YGM_ASSERT_RELEASE(iset.contains(7));
    YGM_ASSERT_RELEASE(iset.contains(100));
    YGM_ASSERT_RELEASE(!iset.contains(3));
  }

  //
  // Test Rank 0 async_insert with ygm set pointer
  {
    ygm::container::set<std::string> sset(world);
    auto                             sset_ptr = sset.get_ygm_ptr();
    if (world.rank() == 0) {
      sset_ptr->async_insert("dog");
      sset_ptr->async_insert("apple");
      sset_ptr->async_insert("red");
    }
    YGM_ASSERT_RELEASE(sset.count("dog") == 1);
    YGM_ASSERT_RELEASE(sset.count("apple") == 1);
    YGM_ASSERT_RELEASE(sset.count("red") == 1);
    YGM_ASSERT_RELEASE(sset.size() == 3);

    // test contains.
    YGM_ASSERT_RELEASE(sset.contains("dog"));
    YGM_ASSERT_RELEASE(sset.contains("apple"));
    YGM_ASSERT_RELEASE(sset.contains("red"));
    YGM_ASSERT_RELEASE(!sset.contains("blue"));
  }

  //
  // Test all ranks async_insert
  {
    ygm::container::set<std::string> sset(world);

    sset.async_insert("dog");
    sset.async_insert("apple");
    sset.async_insert("red");

    YGM_ASSERT_RELEASE(sset.count("dog") == 1);
    YGM_ASSERT_RELEASE(sset.count("apple") == 1);
    YGM_ASSERT_RELEASE(sset.count("red") == 1);
    YGM_ASSERT_RELEASE(sset.size() == 3);
    sset.async_erase("dog");
    YGM_ASSERT_RELEASE(sset.count("dog") == 0);
    YGM_ASSERT_RELEASE(sset.size() == 2);

    // test contains.
    YGM_ASSERT_RELEASE(sset.contains("apple"));
    YGM_ASSERT_RELEASE(sset.contains("red"));
    YGM_ASSERT_RELEASE(!sset.contains("dog"));
    YGM_ASSERT_RELEASE(!sset.contains("blue"));
  }

  //
  // Test async_contains
  {
    static bool              set_contains = false;
    ygm::container::set<int> iset(world);
    int                      val = 42;

    auto f = [](bool& contains, [[maybe_unused]] const int& i) {
      set_contains = contains;
    };

    if (world.rank0()) {
      iset.async_contains(val, f);
    }
    world.barrier();
    YGM_ASSERT_RELEASE(not ygm::logical_or(set_contains, world));

    if (world.rank0()) {
      iset.async_insert(val);
    }
    world.barrier();

    if (world.rank0()) {
      iset.async_contains(val, f);
    }
    world.barrier();
    YGM_ASSERT_RELEASE(ygm::logical_or(set_contains, world));
  }

  //
  // Test async_insert_contains
  {
    static bool                      did_contain = false;
    ygm::container::set<std::string> sset(world);

    auto f = [](bool& contains, [[maybe_unused]] const std::string& s) {
      did_contain = contains;
    };

    if (world.rank0()) {
      sset.async_insert_contains("dog", f);
    }
    world.barrier();
    YGM_ASSERT_RELEASE(not ygm::logical_or(did_contain, world));

    if (world.rank0()) {
      sset.async_insert_contains("dog", f);
    }
    world.barrier();
    YGM_ASSERT_RELEASE(ygm::logical_or(did_contain, world));
  }

  // Test batch erase from set
  {
    int                      num_items   = 100;
    int                      remove_size = 20;
    ygm::container::set<int> iset(world);

    if (world.rank0()) {
      for (int i = 0; i < num_items; ++i) {
        iset.async_insert(i);
      }
    }

    world.barrier();

    YGM_ASSERT_RELEASE(iset.size() == size_t(num_items));

    ygm::container::set<int> to_remove(world);

    if (world.rank0()) {
      for (int i = 0; i < remove_size; ++i) {
        to_remove.async_insert(i);
      }
    }

    world.barrier();

    iset.erase(to_remove);

    iset.for_all([remove_size](const auto& item) {
      YGM_ASSERT_RELEASE(item >= remove_size);
    });

    // test range based loop
    for (auto& item : iset) {
      YGM_ASSERT_RELEASE(item >= remove_size);
    }

    YGM_ASSERT_RELEASE(iset.size() == size_t(num_items - remove_size));
  }

  // Test batch erase from vector
  {
    int                      num_items   = 100;
    int                      remove_size = 20;
    ygm::container::set<int> iset(world);

    if (world.rank0()) {
      for (int i = 0; i < num_items; ++i) {
        iset.async_insert(i);
      }
    }

    world.barrier();

    YGM_ASSERT_RELEASE(iset.size() == size_t(num_items));

    std::vector<int> to_remove;

    if (world.rank0()) {
      for (int i = 0; i < remove_size; ++i) {
        to_remove.push_back(i);
      }
    }

    world.barrier();

    iset.erase(to_remove);

    iset.for_all([remove_size](const auto& item) {
      YGM_ASSERT_RELEASE(item >= remove_size);
    });

    YGM_ASSERT_RELEASE(iset.size() == size_t(num_items - remove_size));
  }

  // Test from bag
  {
    ygm::container::bag<std::string> sbag(
        world, {"one", "two", "three", "one", "two"});
    YGM_ASSERT_RELEASE(sbag.size() == 5);

    ygm::container::set<std::string> sset(world, sbag);
    YGM_ASSERT_RELEASE(sset.size() == 3);
  }

  // Test initializer list
  {
    ygm::container::set<std::string> sset(
        world, {"one", "two", "three", "one", "two"});
    YGM_ASSERT_RELEASE(sset.size() == 3);
  }

  // Test from STL vector
  {
    std::vector<int>         v({1, 2, 3, 4, 5, 1, 1, 1, 3});
    ygm::container::set<int> iset(world, v);
    YGM_ASSERT_RELEASE(iset.size() == 5);
  }

  //
  // Test additional arguments of async_contains
  // {
  //   ygm::container::set<std::string> sset(world);
  //   sset.async_contains("howdy", [](bool c, const std::string s, int i, float
  //   f){}, 3, 3.14); sset.async_contains("howdy", [](auto ptr_set, bool c,
  //   const std::string s){}); world.barrier();
  // }

  //
  // Test swap
  {
    ygm::container::set<std::string> sset(world);
    {
      ygm::container::set<std::string> sset2(world);
      sset2.async_insert("dog");
      sset2.async_insert("apple");
      sset2.async_insert("red");
      sset2.swap(sset);
      YGM_ASSERT_RELEASE(sset2.size() == 0);
    }
    YGM_ASSERT_RELEASE(sset.size() == 3);
    YGM_ASSERT_RELEASE(sset.count("dog") == 1);
    YGM_ASSERT_RELEASE(sset.count("apple") == 1);
    YGM_ASSERT_RELEASE(sset.count("red") == 1);
    sset.async_insert("car");
    YGM_ASSERT_RELEASE(sset.size() == 4);
    YGM_ASSERT_RELEASE(sset.count("car") == 1);

    // test contains.
    YGM_ASSERT_RELEASE(sset.contains("apple"));
    YGM_ASSERT_RELEASE(sset.contains("red"));
    YGM_ASSERT_RELEASE(sset.contains("dog"));
    YGM_ASSERT_RELEASE(sset.contains("car"));
    YGM_ASSERT_RELEASE(!sset.contains("blue"));
  }

  //
  // Test for_all
  {
    ygm::container::set<std::string> sset1(world);
    ygm::container::set<std::string> sset2(world);

    sset1.async_insert("dog");
    sset1.async_insert("apple");
    sset1.async_insert("red");

    sset1.for_all([&sset2](const auto& key) { sset2.async_insert(key); });

    YGM_ASSERT_RELEASE(sset2.count("dog") == 1);
    YGM_ASSERT_RELEASE(sset2.count("apple") == 1);
    YGM_ASSERT_RELEASE(sset2.count("red") == 1);

    // test contains.
    YGM_ASSERT_RELEASE(sset2.contains("apple"));
    YGM_ASSERT_RELEASE(sset2.contains("red"));
    YGM_ASSERT_RELEASE(sset2.contains("dog"));
    YGM_ASSERT_RELEASE(!sset2.contains("blue"));
  }

  // //
  // // Test consume_all
  // {
  //   ygm::container::set<std::string> sset1(world);
  //   ygm::container::set<std::string> sset2(world);

  //   sset1.async_insert("dog");
  //   sset1.async_insert("apple");
  //   sset1.async_insert("red");

  //   sset1.consume_all([&sset2](const auto &key) { sset2.async_insert(key);
  //   });

  //   YGM_ASSERT_RELEASE(sset1.empty());
  //   YGM_ASSERT_RELEASE(sset2.count("dog") == 1);
  //   YGM_ASSERT_RELEASE(sset2.count("apple") == 1);
  //   YGM_ASSERT_RELEASE(sset2.count("red") == 1);
  // }

  // //
  // // Test consume_all_iterative
  // {
  //   ygm::container::set<std::string> sset1(world);
  //   ygm::container::set<std::string> sset2(world);

  //   sset1.async_insert("dog");
  //   sset1.async_insert("apple");
  //   sset1.async_insert("red");

  //   ygm::consume_all_iterative_adapter cai(sset1);
  //   cai.consume_all([&sset2](const auto &key) { sset2.async_insert(key); });

  //   YGM_ASSERT_RELEASE(sset1.empty());
  //   YGM_ASSERT_RELEASE(sset2.count("dog") == 1);
  //   YGM_ASSERT_RELEASE(sset2.count("apple") == 1);
  //   YGM_ASSERT_RELEASE(sset2.count("red") == 1);
  // }

  //
  // Test vector of sets
  {
    int                                   num_sets = 4;
    std::vector<ygm::container::set<int>> vec_sets;

    for (int i = 0; i < num_sets; ++i) {
      vec_sets.emplace_back(world);
    }

    for (int set_index = 0; set_index < num_sets; ++set_index) {
      int item = world.rank() + set_index;
      vec_sets[set_index].async_insert(item);
      vec_sets[set_index].async_insert(item + 1);
    }

    world.barrier();
    for (int set_index = 0; set_index < num_sets; ++set_index) {
      YGM_ASSERT_RELEASE(vec_sets[set_index].size() ==
                         size_t(world.size() + 1));
    }
  }

  //
  // Test copy constructor
  {
    ygm::container::set<int> iset(world);

    int size = 32;

    if (world.rank0()) {
      for (int i = 0; i < size; ++i) {
        iset.async_insert(i);
      }
    }
    world.barrier();

    YGM_ASSERT_RELEASE(iset.size() == size_t(size));

    ygm::container::set<int> iset2(iset);
    YGM_ASSERT_RELEASE(iset.size() == size_t(size));
    YGM_ASSERT_RELEASE(iset2.size() == size_t(size));

    if (world.rank0()) {
      for (int i = 0; i < size; ++i) {
        iset2.async_insert(2 * i + size);
      }
    }
    world.barrier();
    YGM_ASSERT_RELEASE(iset.size() == size_t(size));
    YGM_ASSERT_RELEASE(iset2.size() == size_t(2 * size));
  }

  //
  // Test copy assignment operator
  {
    ygm::container::set<int> iset(world);

    int size = 32;

    if (world.rank0()) {
      for (int i = 0; i < size; ++i) {
        iset.async_insert(i);
      }
    }
    world.barrier();

    YGM_ASSERT_RELEASE(iset.size() == size_t(size));

    ygm::container::set<int> iset2(world);
    iset2 = iset;
    YGM_ASSERT_RELEASE(iset.size() == size_t(size));
    YGM_ASSERT_RELEASE(iset2.size() == size_t(size));

    if (world.rank0()) {
      for (int i = 0; i < size; ++i) {
        iset2.async_insert(2 * i + size);
      }
    }
    world.barrier();
    YGM_ASSERT_RELEASE(iset.size() == size_t(size));
    YGM_ASSERT_RELEASE(iset2.size() == size_t(2 * size));
  }

  //
  // Test move constructor
  {
    ygm::container::set<int> iset(world);

    int size = 32;

    if (world.rank0()) {
      for (int i = 0; i < size; ++i) {
        iset.async_insert(i);
      }
    }
    world.barrier();

    YGM_ASSERT_RELEASE(iset.size() == size_t(size));

    ygm::container::set<int> iset2(std::move(iset));
    YGM_ASSERT_RELEASE(iset.size() == 0);
    YGM_ASSERT_RELEASE(iset2.size() == size_t(size));

    if (world.rank0()) {
      for (int i = 0; i < size; ++i) {
        iset2.async_insert(2 * i + size);
      }
    }
    world.barrier();
    YGM_ASSERT_RELEASE(iset.size() == 0);
    YGM_ASSERT_RELEASE(iset2.size() == size_t(2 * size));
  }

  //
  // Test move constructor
  {
    ygm::container::set<int> iset(world);

    int size = 32;

    if (world.rank0()) {
      for (int i = 0; i < size; ++i) {
        iset.async_insert(i);
      }
    }
    world.barrier();

    YGM_ASSERT_RELEASE(iset.size() == size_t(size));

    ygm::container::set<int> iset2(world);
    iset2 = std::move(iset);
    YGM_ASSERT_RELEASE(iset.size() == 0);
    YGM_ASSERT_RELEASE(iset2.size() == size_t(size));

    if (world.rank0()) {
      for (int i = 0; i < size; ++i) {
        iset2.async_insert(2 * i + size);
      }
    }
    world.barrier();
    YGM_ASSERT_RELEASE(iset.size() == 0);
    YGM_ASSERT_RELEASE(iset2.size() == size_t(2 * size));
  }
  return 0;
}
