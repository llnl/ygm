// Copyright 2019-2021 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG
#include <string>

#include <ygm/comm.hpp>
#include <ygm/container/set.hpp>

int main(int argc, char** argv) {
  ygm::comm world(&argc, &argv);

  //
  // Test Rank 0 async_insert
  {
    ygm::container::multiset<std::string> sset(world);
    if (world.rank() == 0) {
      sset.async_insert("dog");
      sset.async_insert("dog");
      sset.async_insert("apple");
      sset.async_insert("red");
    }
    YGM_ASSERT_RELEASE(sset.count("dog") == 2);
    YGM_ASSERT_RELEASE(sset.count("apple") == 1);
    YGM_ASSERT_RELEASE(sset.count("red") == 1);
    YGM_ASSERT_RELEASE(sset.size() == 4);
    if (world.rank() == 0) {
      sset.async_erase("dog");
    }
    YGM_ASSERT_RELEASE(sset.size() == 2);
    if (world.rank() == 0) {
      sset.async_erase("apple");
    }
    YGM_ASSERT_RELEASE(sset.size() == 1);
    YGM_ASSERT_RELEASE(sset.count("dog") == 0);
    YGM_ASSERT_RELEASE(sset.count("apple") == 0);
  }

  //
  // Test all ranks async_insert
  {
    ygm::container::multiset<std::string> sset(world);

    sset.async_insert("dog");
    sset.async_insert("apple");
    sset.async_insert("red");

    YGM_ASSERT_RELEASE(sset.count("dog") == (size_t)world.size());
    YGM_ASSERT_RELEASE(sset.count("apple") == (size_t)world.size());
    YGM_ASSERT_RELEASE(sset.count("red") == (size_t)world.size());

    sset.async_insert("dog");
    YGM_ASSERT_RELEASE(sset.count("dog") == (size_t)world.size() * 2);
  }

  //
  // Test async_contains
  {
    static bool                   set_contains = false;
    ygm::container::multiset<int> iset(world);
    world.barrier();
    int val = 42;

    auto f = [](bool contains, const int& i) { set_contains = contains; };

    if (world.rank0()) {
      iset.async_contains(val, f);
    }
    world.barrier();
    YGM_ASSERT_RELEASE(not ygm::logical_or(set_contains, world));

    if (world.rank0()) {
      iset.async_insert(val);
    }

    if (world.rank0()) {
      iset.async_contains(val, f);
    }
    world.barrier();
    YGM_ASSERT_RELEASE(ygm::logical_or(set_contains, world));
  }

  //
  // Test async_insert_contains
  {
    static bool                           already_contains = false;
    ygm::container::multiset<std::string> sset(world);
    world.barrier();

    auto f = [](bool& contains, const std::string& s) {
      already_contains = contains;
    };

    if (world.rank0()) {
      sset.async_insert_contains("dog", f);
    }
    world.barrier();
    YGM_ASSERT_RELEASE(not ygm::logical_or(already_contains, world));

    if (world.rank0()) {
      sset.async_insert_contains("dog", f);
    }
    world.barrier();
    YGM_ASSERT_RELEASE(ygm::logical_or(already_contains, world));
  }

  // Test batch erase
  {
    int                           num_items            = 100;
    int                           num_insertion_rounds = 5;
    int                           remove_size          = 20;
    ygm::container::multiset<int> iset(world);

    if (world.rank0()) {
      for (int round = 0; round < num_insertion_rounds; ++round) {
        for (int i = 0; i < num_items; ++i) {
          iset.async_insert(i);
        }
      }
    }

    world.barrier();

    YGM_ASSERT_RELEASE(iset.size() == num_insertion_rounds * num_items);

    ygm::container::set<int> to_remove(world);

    if (world.rank0()) {
      for (int i = 0; i < remove_size; ++i) {
        to_remove.async_insert(i);
      }
    }

    world.barrier();

    iset.erase(to_remove);

    iset.for_all([remove_size, &world](const auto& item) {
      YGM_ASSERT_RELEASE(item >= remove_size);
    });

    YGM_ASSERT_RELEASE(iset.size() ==
                       num_insertion_rounds * (num_items - remove_size));
  }

  // Test batch erase from vector
  {
    int                           num_items            = 100;
    int                           num_insertion_rounds = 5;
    int                           remove_size          = 20;
    ygm::container::multiset<int> iset(world);

    if (world.rank0()) {
      for (int round = 0; round < num_insertion_rounds; ++round) {
        for (int i = 0; i < num_items; ++i) {
          iset.async_insert(i);
        }
      }
    }

    YGM_ASSERT_RELEASE(iset.size() == num_items * num_insertion_rounds);

    std::vector<int> to_remove;

    if (world.rank0()) {
      for (int i = 0; i < remove_size; ++i) {
        to_remove.push_back(i);
      }
    }

    world.barrier();

    iset.erase(to_remove);

    iset.for_all([remove_size, &world](const auto& item) {
      YGM_ASSERT_RELEASE(item >= remove_size);
    });

    YGM_ASSERT_RELEASE(iset.size() ==
                       num_insertion_rounds * (num_items - remove_size));
  }

  //
  // Test swap
  {
    ygm::container::multiset<std::string> sset(world);
    {
      ygm::container::multiset<std::string> sset2(world);
      sset2.async_insert("dog");
      sset2.async_insert("apple");
      sset2.async_insert("red");
      sset2.swap(sset);
      YGM_ASSERT_RELEASE(sset2.size() == 0);
    }
    YGM_ASSERT_RELEASE(sset.size() == 3 * (size_t)world.size());
    YGM_ASSERT_RELEASE(sset.count("dog") == (size_t)world.size());
    YGM_ASSERT_RELEASE(sset.count("apple") == (size_t)world.size());
    YGM_ASSERT_RELEASE(sset.count("red") == (size_t)world.size());
    sset.async_insert("car");
    YGM_ASSERT_RELEASE(sset.size() == 4 * (size_t)world.size());
    YGM_ASSERT_RELEASE(sset.count("car") == (size_t)world.size());
  }

  //
  // Test for_all
  {
    ygm::container::multiset<std::string> sset1(world);
    ygm::container::multiset<std::string> sset2(world);

    sset1.async_insert("dog");
    sset1.async_insert("apple");
    sset1.async_insert("red");

    sset1.for_all([&sset2](const auto& key) { sset2.async_insert(key); });

    YGM_ASSERT_RELEASE(sset2.count("dog") == world.size());
    YGM_ASSERT_RELEASE(sset2.count("apple") == world.size());
    YGM_ASSERT_RELEASE(sset2.count("red") == world.size());
  }

  //
  // Test vector of sets
  {
    int                                        num_sets = 4;
    std::vector<ygm::container::multiset<int>> vec_sets;

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
      YGM_ASSERT_RELEASE(vec_sets[set_index].size() == world.size() * 2);
    }
  }

  //
  // Test copy constructor
  {
    ygm::container::multiset<int> mset(world);

    int size = 32;

    if (world.rank0()) {
      for (int i = 0; i < size; ++i) {
        mset.async_insert(1);
      }
    }
    world.barrier();

    YGM_ASSERT_RELEASE(mset.size() == size);

    ygm::container::multiset<int> mset2(mset);
    YGM_ASSERT_RELEASE(mset.size() == size);
    YGM_ASSERT_RELEASE(mset2.size() == size);

    if (world.rank0()) {
      for (int i = 0; i < size; ++i) {
        mset2.async_insert(2 * i);
      }
    }
    world.barrier();
    YGM_ASSERT_RELEASE(mset.size() == size);
    YGM_ASSERT_RELEASE(mset2.size() == 2 * size);
  }

  //
  // Test copy assignment operator
  {
    ygm::container::multiset<int> mset(world);

    int size = 32;

    if (world.rank0()) {
      for (int i = 0; i < size; ++i) {
        mset.async_insert(1);
      }
    }
    world.barrier();

    YGM_ASSERT_RELEASE(mset.size() == size);

    ygm::container::multiset<int> mset2(world);
    mset2 = mset;
    YGM_ASSERT_RELEASE(mset.size() == size);
    YGM_ASSERT_RELEASE(mset2.size() == size);

    if (world.rank0()) {
      for (int i = 0; i < size; ++i) {
        mset2.async_insert(2 * i);
      }
    }
    world.barrier();
    YGM_ASSERT_RELEASE(mset.size() == size);
    YGM_ASSERT_RELEASE(mset2.size() == 2 * size);
  }

  //
  // Test move constructor
  {
    ygm::container::multiset<int> mset(world);

    int size = 32;

    if (world.rank0()) {
      for (int i = 0; i < size; ++i) {
        mset.async_insert(1);
      }
    }
    world.barrier();

    YGM_ASSERT_RELEASE(mset.size() == size);

    ygm::container::multiset<int> mset2(std::move(mset));
    YGM_ASSERT_RELEASE(mset.size() == 0);
    YGM_ASSERT_RELEASE(mset2.size() == size);

    if (world.rank0()) {
      for (int i = 0; i < size; ++i) {
        mset2.async_insert(2 * i);
      }
    }
    world.barrier();
    YGM_ASSERT_RELEASE(mset.size() == 0);
    YGM_ASSERT_RELEASE(mset2.size() == 2 * size);
  }

  //
  // Test move constructor
  {
    ygm::container::multiset<int> mset(world);

    int size = 32;

    if (world.rank0()) {
      for (int i = 0; i < size; ++i) {
        mset.async_insert(1);
      }
    }
    world.barrier();

    YGM_ASSERT_RELEASE(mset.size() == size);

    ygm::container::multiset<int> mset2(world);
    mset2 = std::move(mset);
    YGM_ASSERT_RELEASE(mset.size() == 0);
    YGM_ASSERT_RELEASE(mset2.size() == size);

    if (world.rank0()) {
      for (int i = 0; i < size; ++i) {
        mset2.async_insert(2 * i);
      }
    }
    world.barrier();
    YGM_ASSERT_RELEASE(mset.size() == 0);
    YGM_ASSERT_RELEASE(mset2.size() == 2 * size);
  }

  return 0;
}
