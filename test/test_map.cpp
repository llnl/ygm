// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG
#include <algorithm>
#include <string>
#include <ygm/comm.hpp>
#include <ygm/container/bag.hpp>
#include <ygm/container/map.hpp>
#include <ygm/container/set.hpp>

int main(int argc, char **argv) {
  ygm::comm world(&argc, &argv);

  // Test basic tagging
  {
    ygm::container::map<std::string, int> smap(world);

    static_assert(std::is_same_v<decltype(smap)::self_type, decltype(smap)>);
    static_assert(std::is_same_v<decltype(smap)::mapped_type, int>);
    static_assert(std::is_same_v<decltype(smap)::key_type, std::string>);
    static_assert(std::is_same_v<decltype(smap)::size_type, size_t>);
    static_assert(
        std::is_same_v<
            decltype(smap)::for_all_args,
            std::tuple<decltype(smap)::key_type, decltype(smap)::mapped_type>>);
  }

  //
  // Test Rank 0 async_insert
  {
    ygm::container::map<std::string, std::string> smap(world);
    if (world.rank() == 0) {
      smap.async_insert("dog", "cat");
      smap.async_insert("apple", "orange");
      smap.async_insert("red", "green");
    }
    YGM_ASSERT_RELEASE(smap.count("dog") == 1);
    YGM_ASSERT_RELEASE(smap.count("apple") == 1);
    YGM_ASSERT_RELEASE(smap.count("red") == 1);

    // test contains.
    YGM_ASSERT_RELEASE(smap.contains("dog"));
    YGM_ASSERT_RELEASE(smap.contains("apple"));
    YGM_ASSERT_RELEASE(smap.contains("red"));
    YGM_ASSERT_RELEASE(!smap.contains("blue"));
  }

  //
  // Test all ranks async_insert
  {
    ygm::container::map<std::string, std::string> smap(world);

    smap.async_insert("dog", "cat");
    smap.async_insert("apple", "orange");
    smap.async_insert("red", "green");

    YGM_ASSERT_RELEASE(smap.count("dog") == 1);
    YGM_ASSERT_RELEASE(smap.count("apple") == 1);
    YGM_ASSERT_RELEASE(smap.count("red") == 1);

    // test contains.
    YGM_ASSERT_RELEASE(smap.contains("dog"));
    YGM_ASSERT_RELEASE(smap.contains("apple"));
    YGM_ASSERT_RELEASE(smap.contains("red"));
    YGM_ASSERT_RELEASE(!smap.contains("blue"));
  }

  //
  // Test async_visit & async_visit const
  {
    ygm::container::map<std::string, std::string> smap(world);

    smap.async_insert("dog", "cat");
    smap.async_insert("apple", "orange");

    world.barrier();

    smap.async_insert("dog", "dog");
    smap.async_insert("red", "green");

    world.barrier();

    smap.async_visit("dog", []([[maybe_unused]] const auto &key, auto &value) {
      YGM_ASSERT_RELEASE(value == "cat");
    });

    smap.async_visit_if_contains("apple",
                                 []([[maybe_unused]] auto key, auto &value) {
                                   YGM_ASSERT_RELEASE(value == "orange");
                                 });

    const ygm::container::map<std::string, std::string> &csmap = smap;
    csmap.async_visit_if_contains("red",
                                  []([[maybe_unused]] auto key, auto &value) {
                                    YGM_ASSERT_RELEASE(value == "green");
                                  });

    smap.async_visit_if_contains(
        "SHOULD_BE_MISSING",
        []([[maybe_unused]] auto key, [[maybe_unused]] auto &value) {
          YGM_ASSERT_RELEASE(false);
        });
  }

  //
  // Test async_visit with functor
  {
    ygm::container::map<std::string, std::string> smap(world);

    smap.async_insert("dog", "cat");
    smap.async_insert("apple", "orange");

    world.barrier();

    smap.async_insert("dog", "dog");
    smap.async_insert("red", "green");

    world.barrier();

    struct dog_check {
      void operator()([[maybe_unused]] const std::string &key,
                      std::string                        &value) {
        YGM_ASSERT_RELEASE(value == "cat");
      }
    };

    smap.async_visit("dog", dog_check());
  }

  //
  // Test all ranks default & async_visit_if_contains
  {
    ygm::container::map<std::string, std::string> smap(world);
    smap.async_visit("dog",
                     [](const std::string &key, const std::string &value) {
                       YGM_ASSERT_RELEASE(key == "dog");
                       YGM_ASSERT_RELEASE(value == "");
                     });
    smap.async_visit("cat", [](const std::string &key, std::string &value) {
      YGM_ASSERT_RELEASE(key == "cat");
      YGM_ASSERT_RELEASE(value == "");
    });
    smap.async_visit_if_contains("red", []([[maybe_unused]] const auto &k,
                                           [[maybe_unused]] const auto &v) {
      YGM_ASSERT_RELEASE(false);
    });

    YGM_ASSERT_RELEASE(smap.count("dog") == 1);
    YGM_ASSERT_RELEASE(smap.count("cat") == 1);
    YGM_ASSERT_RELEASE(smap.count("red") == 0);

    YGM_ASSERT_RELEASE(smap.size() == 2);

    // test contains.
    YGM_ASSERT_RELEASE(smap.contains("dog"));
    YGM_ASSERT_RELEASE(smap.contains("cat"));
    YGM_ASSERT_RELEASE(!smap.contains("red"));
    YGM_ASSERT_RELEASE(!smap.contains("blue"));

    if (world.rank() == 0) {
      smap.async_erase("dog");
    }
    YGM_ASSERT_RELEASE(smap.count("dog") == 0);
    YGM_ASSERT_RELEASE(smap.size() == 1);
    YGM_ASSERT_RELEASE(!smap.contains("dog"));
    smap.async_erase("cat");
    YGM_ASSERT_RELEASE(smap.count("cat") == 0);
    YGM_ASSERT_RELEASE(!smap.contains("cat"));

    YGM_ASSERT_RELEASE(smap.size() == 0);
  }

  //
  // Test default value
  {
    ygm::container::map<std::string, std::string> smap(world, "NOT FOUND");

    smap.async_insert("dog", "cat");

    world.barrier();

    if (world.rank0()) {
      smap.async_visit("dog",
                       []([[maybe_unused]] const auto &k, const auto &v) {
                         YGM_ASSERT_RELEASE(v == "cat");
                       });
      smap.async_visit("not inserted",
                       []([[maybe_unused]] const auto &k, const auto &v) {
                         YGM_ASSERT_RELEASE(v == "NOT FOUND");
                       });
    }

    YGM_ASSERT_RELEASE(smap.size() == 2);
  }

  // //
  // // Test async_insert_else_visit
  // {
  //   ygm::container::map<std::string, std::string> smap(world);

  //   smap.async_insert("dog", "cat");

  //   world.barrier();

  //   static int dog_visit_counter{0};

  //   smap.async_insert_else_visit(
  //       "dog", "other_dog",
  //       [](const auto &key, const auto &value, const auto &new_value) {
  //         dog_visit_counter++;
  //       });

  //   world.barrier();

  //   YGM_ASSERT_RELEASE(sum(dog_visit_counter, world) ==
  //   world.size());

  //   static int apple_visit_counter{0};

  //   smap.async_insert_else_visit(
  //       "apple", "orange",
  //       [](const auto &key, const auto &value, const auto &new_value) {
  //         apple_visit_counter++;
  //       });

  //   world.barrier();

  //   YGM_ASSERT_RELEASE(sum(apple_visit_counter, world) ==
  //                  world.size() - 1);

  //   if (world.rank0()) {
  //     smap.async_insert_else_visit(
  //         "red", "green",
  //         [](const auto &key, const auto &value, const auto &new_value) {
  //           YGM_ASSERT_RELEASE(true == false);
  //         });
  //   }
  // }

  //
  // Test async_reduce
  {
    ygm::container::map<std::string, int> smap(world);

    int num_reductions = 5;
    for (int i = 0; i < num_reductions; ++i) {
      smap.async_reduce("sum", i, std::plus<int>());
      smap.async_reduce("min", i, [](const int &a, const int &b) {
        return std::min<int>(a, b);
      });
      smap.async_reduce("max", i, [](const int &a, const int &b) {
        return std::max<int>(a, b);
      });
    }

    world.barrier();

    smap.for_all([&world, &num_reductions](const auto &key, const auto &value) {
      if (key == "sum") {
        YGM_ASSERT_RELEASE(value == world.size() * num_reductions *
                                        (num_reductions - 1) / 2);
      } else if (key == "min") {
        YGM_ASSERT_RELEASE(value == 0);
      } else if (key == "max") {
        YGM_ASSERT_RELEASE(value == num_reductions - 1);
      } else {
        YGM_ASSERT_RELEASE(false);
      }
    });
  }

  //
  // Test swap & async_insert_or_assign
  {
    ygm::container::map<std::string, std::string> smap(world);
    {
      ygm::container::map<std::string, std::string> smap2(world);
      smap2.async_insert("dog", "cat");
      smap2.async_insert("apple", "orange");
      smap2.async_insert("red", "green");
      smap2.swap(smap);
      YGM_ASSERT_RELEASE(smap2.size() == 0);
    }
    YGM_ASSERT_RELEASE(smap.size() == 3);
    YGM_ASSERT_RELEASE(smap.count("dog") == 1);
    YGM_ASSERT_RELEASE(smap.count("apple") == 1);
    YGM_ASSERT_RELEASE(smap.count("red") == 1);

    YGM_ASSERT_RELEASE(!smap.contains("car"));
    smap.async_insert_or_assign("car", "truck");
    YGM_ASSERT_RELEASE(smap.size() == 4);
    YGM_ASSERT_RELEASE(smap.count("car") == 1);

    // test contains.
    YGM_ASSERT_RELEASE(smap.contains("dog"));
    YGM_ASSERT_RELEASE(smap.contains("car"));
    YGM_ASSERT_RELEASE(smap.contains("red"));
    YGM_ASSERT_RELEASE(!smap.contains("blue"));
  }

  // Test batch erase from set
  {
    int                           num_items   = 100;
    int                           remove_size = 20;
    ygm::container::map<int, int> imap(world);

    if (world.rank0()) {
      for (int i = 0; i < num_items; ++i) {
        imap.async_insert(i, i);
      }
    }

    world.barrier();

    YGM_ASSERT_RELEASE(imap.size() == size_t(num_items));

    ygm::container::set<int> to_remove(world);

    if (world.rank0()) {
      for (int i = 0; i < remove_size; ++i) {
        to_remove.async_insert(i);
      }
    }

    world.barrier();

    imap.erase(to_remove);

    imap.for_all(
        [remove_size](const auto &key, [[maybe_unused]] const auto &value) {
          YGM_ASSERT_RELEASE(key >= remove_size);
        });

    // testing range based loop
    for (auto &kv : imap) {
      YGM_ASSERT_RELEASE(kv.first >= remove_size);
    }

    YGM_ASSERT_RELEASE(imap.size() == size_t(num_items - remove_size));
  }

  // Test batch erase from map
  {
    int                           num_items   = 100;
    int                           remove_size = 20;
    ygm::container::map<int, int> imap(world);

    if (world.rank0()) {
      for (int i = 0; i < num_items; ++i) {
        imap.async_insert(i, i);
      }
    }

    world.barrier();

    YGM_ASSERT_RELEASE(imap.size() == size_t(num_items));

    ygm::container::map<int, int> to_remove(world);

    if (world.rank0()) {
      for (int i = 0; i < remove_size; ++i) {
        to_remove.async_insert(i, i + (i % 2));
      }
    }

    world.barrier();

    imap.erase(to_remove);

    imap.for_all(
        [remove_size](const auto &key, [[maybe_unused]] const auto &value) {
          YGM_ASSERT_RELEASE(((key % 2) == 1) || (key >= remove_size));
        });

    YGM_ASSERT_RELEASE(imap.size() == size_t(num_items - remove_size / 2));
  }

  // Test batch erase from vector
  {
    int                           num_items   = 100;
    int                           remove_size = 20;
    ygm::container::map<int, int> imap(world);

    if (world.rank0()) {
      for (int i = 0; i < num_items; ++i) {
        imap.async_insert(i, i);
      }
    }

    world.barrier();

    YGM_ASSERT_RELEASE(imap.size() == size_t(num_items));

    std::vector<int> to_remove;

    if (world.rank0()) {
      for (int i = 0; i < remove_size; ++i) {
        to_remove.push_back(i);
      }
    }

    world.barrier();

    imap.erase(to_remove);

    imap.for_all(
        [remove_size](const auto &key, [[maybe_unused]] const auto &value) {
          YGM_ASSERT_RELEASE(key >= remove_size);
        });

    YGM_ASSERT_RELEASE(imap.size() == size_t(num_items - remove_size));
  }

  // Test batch erase from vector of keys and values
  {
    int                           num_items   = 100;
    int                           remove_size = 20;
    ygm::container::map<int, int> imap(world);

    if (world.rank0()) {
      for (int i = 0; i < num_items; ++i) {
        imap.async_insert(i, i);
      }
    }

    world.barrier();

    YGM_ASSERT_RELEASE(imap.size() == size_t(num_items));

    std::vector<std::pair<int, int>> to_remove;

    if (world.rank0()) {
      for (int i = 0; i < remove_size; ++i) {
        to_remove.push_back(std::make_pair(i, i + (i % 2)));
      }
    }

    world.barrier();

    imap.erase(to_remove);

    imap.for_all(
        [remove_size](const auto &key, [[maybe_unused]] const auto &value) {
          YGM_ASSERT_RELEASE(((key % 2) == 1) || (key >= remove_size));
        });

    YGM_ASSERT_RELEASE(imap.size() == size_t(num_items - remove_size / 2));
  }

  // Test batch erase from bag of keys and values
  {
    int                           num_items   = 100;
    int                           remove_size = 20;
    ygm::container::map<int, int> imap(world);

    if (world.rank0()) {
      for (int i = 0; i < num_items; ++i) {
        imap.async_insert(i, i);
      }
    }

    world.barrier();

    YGM_ASSERT_RELEASE(imap.size() == size_t(num_items));

    ygm::container::bag<std::pair<int, int>> to_remove(world);

    if (world.rank0()) {
      for (int i = 0; i < remove_size; ++i) {
        to_remove.async_insert(std::make_pair(i, i + (i % 2)));
      }
    }

    world.barrier();

    imap.erase(to_remove);

    imap.for_all(
        [remove_size](const auto &key, [[maybe_unused]] const auto &value) {
          YGM_ASSERT_RELEASE(((key % 2) == 1) || (key >= remove_size));
        });

    YGM_ASSERT_RELEASE(imap.size() == size_t(num_items - remove_size / 2));
  }

  //
  // Test map<vector>
  {
    ygm::container::map<std::string, std::vector<std::string>> smap(world);
    auto str_push_back = []([[maybe_unused]] const auto &key, auto &value,
                            const std::string &str) {
      // auto str_push_back = [](auto key_value, const std::string &str) {
      value.push_back(str);
    };
    if (world.rank0()) {
      smap.async_visit("foo", str_push_back, std::string("bar"));
      smap.async_visit("foo", str_push_back, std::string("baz"));
    }

    std::vector<std::string> gather_list = {"foo"};

    if (!world.rank0()) {
      gather_list.clear();
    }

    auto gmap = smap.gather_keys(gather_list);

    if (world.rank0()) {
      YGM_ASSERT_RELEASE(gmap["foo"][0] == "bar");
      YGM_ASSERT_RELEASE(gmap["foo"][1] == "baz");
    } else {
      YGM_ASSERT_RELEASE(gmap["foo"].empty());
    }
  }

  //
  // Test for_all
  {
    ygm::container::map<std::string, std::string> smap1(world);
    ygm::container::map<std::string, std::string> smap2(world);

    smap1.async_insert("dog", "cat");
    smap1.async_insert("apple", "orange");
    smap1.async_insert("red", "green");

    smap1.for_all([&smap2](const auto &key, const auto &value) {
      smap2.async_insert(key, value);
    });

    YGM_ASSERT_RELEASE(smap2.count("dog") == 1);
    YGM_ASSERT_RELEASE(smap2.count("apple") == 1);
    YGM_ASSERT_RELEASE(smap2.count("red") == 1);
  }

  //
  // Test gather
  {
    ygm::container::map<std::string, std::string> smap(world);

    smap.async_insert("dog", "cat");
    smap.async_insert("apple", "orange");
    smap.async_insert("red", "green");

    {
      std::map<std::string, std::string> local_map;
      smap.gather(local_map, 0);
      if (world.rank0()) {
        YGM_ASSERT_RELEASE(local_map.size() == 3);
        YGM_ASSERT_RELEASE(local_map["dog"] == "cat");
        YGM_ASSERT_RELEASE(local_map["apple"] == "orange");
        YGM_ASSERT_RELEASE(local_map["red"] == "green");
      }
    }
    {
      std::map<std::string, std::string> local_map;
      smap.gather(local_map);
      YGM_ASSERT_RELEASE(local_map.size() == 3);
      YGM_ASSERT_RELEASE(local_map["dog"] == "cat");
      YGM_ASSERT_RELEASE(local_map["apple"] == "orange");
      YGM_ASSERT_RELEASE(local_map["red"] == "green");
    }
  }

  // Test copy constructor
  {
    ygm::container::map<std::string, std::string> smap(world);
    if (world.rank0()) {
      smap.async_insert("dog", "cat");
      smap.async_insert("apple", "orange");
      smap.async_insert("red", "green");
    }
    world.barrier();

    ygm::container::map<std::string, std::string> smap2(smap);

    YGM_ASSERT_RELEASE(smap.size() == 3);
    YGM_ASSERT_RELEASE(smap2.size() == 3);

    if (world.rank0()) {
      smap2.async_insert("up", "down");
    }
    world.barrier();

    YGM_ASSERT_RELEASE(smap.size() == 3);
    YGM_ASSERT_RELEASE(smap2.size() == 4);
  }

  // Test copy assignment operator
  {
    ygm::container::map<std::string, std::string> smap(world);
    if (world.rank0()) {
      smap.async_insert("dog", "cat");
      smap.async_insert("apple", "orange");
      smap.async_insert("red", "green");
    }
    world.barrier();

    ygm::container::map<std::string, std::string> smap2(world);
    smap2 = smap;

    YGM_ASSERT_RELEASE(smap.size() == 3);
    YGM_ASSERT_RELEASE(smap2.size() == 3);

    if (world.rank0()) {
      smap2.async_insert("up", "down");
    }
    world.barrier();

    YGM_ASSERT_RELEASE(smap.size() == 3);
    YGM_ASSERT_RELEASE(smap2.size() == 4);
  }

  // Test move constructor
  {
    ygm::container::map<std::string, std::string> smap(world);
    if (world.rank0()) {
      smap.async_insert("dog", "cat");
      smap.async_insert("apple", "orange");
      smap.async_insert("red", "green");
    }
    world.barrier();

    ygm::container::map<std::string, std::string> smap2(std::move(smap));

    YGM_ASSERT_RELEASE(smap.size() ==
                       0);  // I don't think this is guaranteed for a move. smap
                            // will be in some undefined state
    YGM_ASSERT_RELEASE(smap2.size() == 3);

    if (world.rank0()) {
      smap2.async_insert("up", "down");
    }
    world.barrier();

    YGM_ASSERT_RELEASE(smap.size() == 0);
    YGM_ASSERT_RELEASE(smap2.size() == 4);
  }

  // Test move assignment operator
  {
    ygm::container::map<std::string, std::string> smap(world);
    if (world.rank0()) {
      smap.async_insert("dog", "cat");
      smap.async_insert("apple", "orange");
      smap.async_insert("red", "green");
    }
    world.barrier();

    ygm::container::map<std::string, std::string> smap2(world);
    smap2 = std::move(smap);

    YGM_ASSERT_RELEASE(smap.size() ==
                       0);  // I don't think this is guaranteed for a move. smap
                            // will be in some undefined state
    YGM_ASSERT_RELEASE(smap2.size() == 3);

    if (world.rank0()) {
      smap2.async_insert("up", "down");
    }
    world.barrier();

    YGM_ASSERT_RELEASE(smap.size() == 0);
    YGM_ASSERT_RELEASE(smap2.size() == 4);
  }

  return 0;
}
