// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG

#include <ygm/comm.hpp>
#include <ygm/container/array.hpp>
#include <ygm/container/bag.hpp>
#include <ygm/container/map.hpp>

#include <map>
#include <vector>

int main(int argc, char **argv) {
  ygm::comm world(&argc, &argv);

  // Test basic tagging
  {
    int                        size = 64;
    ygm::container::array<int> arr(world, size);

    static_assert(std::is_same_v<decltype(arr)::self_type, decltype(arr)>);
    static_assert(std::is_same_v<decltype(arr)::mapped_type, decltype(size)>);
    static_assert(std::is_same_v<decltype(arr)::key_type, size_t>);
    static_assert(
        std::is_same_v<decltype(arr)::size_type, decltype(arr)::key_type>);
    static_assert(
        std::is_same_v<
            decltype(arr)::for_all_args,
            std::tuple<decltype(arr)::key_type, decltype(arr)::mapped_type>>);
  }

  // Test async_set
  {
    int                        size = 64;
    ygm::container::array<int> arr(world, size);

    if (world.rank0()) {
      for (int i = 0; i < size; ++i) {
        arr.async_set(i, i);
      }
    }

    arr.for_all([](const auto index, const auto value) {
      YGM_ASSERT_RELEASE(index == size_t(value));
    });

    // test range-based for
    for (const auto &index_item : arr) {
      YGM_ASSERT_RELEASE(index_item.index == size_t(index_item.value));
    }

    for (auto iter = arr.cbegin(); iter != arr.cend(); ++iter) {
      YGM_ASSERT_RELEASE(iter->index == size_t(iter->value));
    }

    auto const_for_loop = [](const ygm::container::array<int> &c_arr) {
      for (const auto &index_item : c_arr) {
        YGM_ASSERT_RELEASE(index_item.index == size_t(index_item.value));
      }
    };
    const_for_loop(arr);
  }

  // Test async_binary_op_update_value
  {
    int size = 32;

    ygm::container::array<int> arr(world, size);

    if (world.rank0()) {
      for (int i = 0; i < size; ++i) {
        arr.async_set(i, i);
      }
    }

    world.barrier();

    for (int i = 0; i < size; ++i) {
      arr.async_binary_op_update_value(i, 2, std::plus<int>());
    }

    arr.for_all([&world](const auto index, const auto value) {
      YGM_ASSERT_RELEASE(size_t(value) == index + 2 * world.size());
    });
  }

  // Test async_bit_xor
  {
    int size = 64;

    ygm::container::array<int> arr(world, size);

    if (world.rank0()) {
      for (int i = 0; i < size; ++i) {
        arr.async_set(i, i);
      }
    }

    world.barrier();

    for (int i = 0; i < size; ++i) {
      arr.async_bit_xor(i, world.rank());
    }

    arr.for_all([&world](const auto index, const auto value) {
      int cumulative_xor{-1};
      switch ((world.size() - 1) % 4) {
        case 0:
          cumulative_xor = world.size() - 1;
          break;
        case 1:
          cumulative_xor = 1;
          break;
        case 2:
          cumulative_xor = world.size();
          break;
        case 3:
          cumulative_xor = 0;
          break;
      }
      YGM_ASSERT_RELEASE(size_t(value) == (index ^ cumulative_xor));
    });
  }

  // Test async_increment
  {
    int size = 64;

    ygm::container::array<int> arr(world, size);

    if (world.rank0()) {
      for (int i = 0; i < size; ++i) {
        arr.async_set(i, i);
      }
    }

    world.barrier();

    for (int i = 0; i < size; ++i) {
      arr.async_increment(i);
    }

    arr.for_all([&world](const auto index, const auto value) {
      YGM_ASSERT_RELEASE(size_t(value) == index + world.size());
    });
  }

  // Test async_visit
  {
    int size = 64;

    ygm::container::array<int> arr(world, size);

    if (world.rank0()) {
      for (int i = 0; i < size; ++i) {
        arr.async_set(i, i);
      }
    }

    world.barrier();

    for (int i = 0; i < size; ++i) {
      arr.async_visit(i, [](const auto index, const auto value) {
        YGM_ASSERT_RELEASE(size_t(value) == index);
      });
    }
  }

  // Test async_visit (ptr)
  {
    int size = 64;

    ygm::container::array<int> arr(world, size);

    if (world.rank0()) {
      for (int i = 0; i < size; ++i) {
        arr.async_set(i, i);
      }
    }

    world.barrier();

    for (int i = 0; i < size; ++i) {
      arr.async_visit(i, [](const auto index, const auto value) {
        YGM_ASSERT_RELEASE(size_t(value) == index);
      });
    }
  }

  // Test async_visit functor
  {
    struct visit_functor {
      void operator()(const size_t index, const int value) {
        YGM_ASSERT_RELEASE(size_t(value) == index);
      }
    };

    int size = 64;

    ygm::container::array<int> arr(world, size);

    if (world.rank0()) {
      for (int i = 0; i < size; ++i) {
        arr.async_set(i, i);
      }
    }

    world.barrier();

    for (int i = 0; i < size; ++i) {
      arr.async_visit(i, visit_functor());
    }
  }

  //
  // Test async_reduce
  {
    ygm::container::array<int> arr(world, 3);

    int num_reductions = 5;
    for (int i = 0; i < num_reductions; ++i) {
      arr.async_reduce(0, i, std::plus<int>());
      arr.async_reduce(
          1, i, [](const int &a, const int &b) { return std::min<int>(a, b); });
      arr.async_reduce(
          2, i, [](const int &a, const int &b) { return std::max<int>(a, b); });
    }

    world.barrier();

    arr.for_all(
        [&world, &num_reductions](const auto &index, const auto &value) {
          if (index == 0) {
            YGM_ASSERT_RELEASE(value == world.size() * num_reductions *
                                            (num_reductions - 1) / 2);
          } else if (index == 1) {
            YGM_ASSERT_RELEASE(value == 0);
          } else if (index == 2) {
            YGM_ASSERT_RELEASE(value == num_reductions - 1);
          } else {
            YGM_ASSERT_RELEASE(false);
          }
        });
  }

  // Test value-only for_all
  {
    int size = 64;

    ygm::container::array<int> arr(world, size);

    if (world.rank0()) {
      for (int i = 0; i < size; ++i) {
        arr.async_set(i, 1);
      }
    }

    world.barrier();

    for (int i = 0; i < size; ++i) {
      arr.async_increment(i);
    }

    arr.for_all([&world](const auto value) {
      YGM_ASSERT_RELEASE(value == world.size() + 1);
    });
  }

  // Test small array
  {
    int                        size = 1;
    ygm::container::array<int> arr(world, size);

    if (world.rank0()) {
      for (int i = 0; i < size; ++i) {
        arr.async_set(i, i);
      }
    }

    arr.for_all([](const auto index, const auto value) {
      YGM_ASSERT_RELEASE(index == size_t(value));
    });
  }

  // Test copy constructor
  {
    int size = 64;

    ygm::container::array<int> arr(world, size);

    if (world.rank0()) {
      for (int i = 0; i < size; ++i) {
        arr.async_set(i, 2 * i);
      }
    }

    world.barrier();

    ygm::container::array<int> arr_copy(arr);

    arr_copy.for_all([&arr](const auto &index, const auto &value) {
      arr.async_visit(
          index,
          []([[maybe_unused]] const auto &index, const auto &my_value,
             const auto &other_value) {
            YGM_ASSERT_RELEASE(my_value == other_value);
          },
          value);
    });

    arr.for_all([&arr_copy](const auto &index, const auto &value) {
      arr_copy.async_visit(
          index,
          []([[maybe_unused]] const auto &index, const auto &my_value,
             const auto &other_value) {
            YGM_ASSERT_RELEASE(my_value == other_value);
          },
          value);
    });

    // Double all values in copy
    arr_copy.for_all(
        []([[maybe_unused]] const auto &index, auto &value) { value *= 2; });

    world.barrier();

    arr.for_all([](const auto &index, const auto &value) {
      YGM_ASSERT_RELEASE(size_t(value) == 2 * index);
    });

    arr_copy.for_all([](const auto &index, const auto &value) {
      YGM_ASSERT_RELEASE(size_t(value) == 4 * index);
    });
  }

  // Test copy assignment operator
  {
    int size = 64;

    ygm::container::array<int> arr(world, size);

    if (world.rank0()) {
      for (int i = 0; i < size; ++i) {
        arr.async_set(i, 2 * i);
      }
    }

    world.barrier();

    ygm::container::array<int> arr_copy(world, 0);
    arr_copy = arr;

    arr_copy.for_all([&arr](const auto &index, const auto &value) {
      arr.async_visit(
          index,
          []([[maybe_unused]] const auto &index, const auto &my_value,
             const auto &other_value) {
            YGM_ASSERT_RELEASE(my_value == other_value);
          },
          value);
    });

    arr.for_all([&arr_copy](const auto &index, const auto &value) {
      arr_copy.async_visit(
          index,
          []([[maybe_unused]] const auto &index, const auto &my_value,
             const auto &other_value) {
            YGM_ASSERT_RELEASE(my_value == other_value);
          },
          value);
    });

    // Double all values in copy
    arr_copy.for_all(
        []([[maybe_unused]] const auto &index, auto &value) { value *= 2; });

    world.barrier();

    arr.for_all([](const auto &index, const auto &value) {
      YGM_ASSERT_RELEASE(size_t(value) == 2 * index);
    });

    arr_copy.for_all([](const auto &index, const auto &value) {
      YGM_ASSERT_RELEASE(size_t(value) == 4 * index);
    });
  }

  // Test move constructor
  {
    int size = 64;

    ygm::container::array<int> arr(world, size);

    if (world.rank0()) {
      for (int i = 0; i < size; ++i) {
        arr.async_set(i, 2 * i);
      }
    }

    world.barrier();

    ygm::container::array<int> arr2(std::move(arr));

    world.barrier();

    YGM_ASSERT_RELEASE(arr.size() == 0);
    YGM_ASSERT_RELEASE(arr2.size() == size_t(size));

    arr2.for_all([](const auto &index, const auto &value) {
      YGM_ASSERT_RELEASE(size_t(value) == 2 * index);
    });
  }

  // Test move assignment operator
  {
    int size = 64;

    ygm::container::array<int> arr(world, size);

    if (world.rank0()) {
      for (int i = 0; i < size; ++i) {
        arr.async_set(i, 2 * i);
      }
    }

    world.barrier();

    ygm::container::array<int> arr2(world, 0);
    arr2 = std::move(arr);

    world.barrier();

    YGM_ASSERT_RELEASE(arr.size() == 0);
    YGM_ASSERT_RELEASE(arr2.size() == size_t(size));

    arr2.for_all([](const auto &index, const auto &value) {
      YGM_ASSERT_RELEASE(size_t(value) == 2 * index);
    });
  }

  // Test resize
  {
    int large_size = 64;
    int small_size = 32;

    ygm::container::array<int> arr(world, large_size);

    if (world.rank0()) {
      for (int i = 0; i < large_size; ++i) {
        arr.async_set(i, 2 * i);
      }
    }

    world.barrier();

    YGM_ASSERT_RELEASE(arr.size() == size_t(large_size));
    arr.for_all([](const auto &index, const auto &value) {
      YGM_ASSERT_RELEASE(size_t(value) == 2 * index);
    });

    arr.resize(small_size);

    YGM_ASSERT_RELEASE(arr.size() == size_t(small_size));
    arr.for_all([](const auto &index, const auto &value) {
      YGM_ASSERT_RELEASE(size_t(value) == 2 * index);
    });

    arr.resize(large_size);

    YGM_ASSERT_RELEASE(arr.size() == size_t(large_size));
    arr.for_all([&small_size](const auto &index, const auto &value) {
      if (index < size_t(small_size)) {
        YGM_ASSERT_RELEASE(size_t(value) == 2 * index);
      }
    });
  }

  // Test clear
  {
    int initial_size = 64;

    ygm::container::array<int> arr(world, initial_size);

    if (world.rank0()) {
      for (int i = 0; i < initial_size; ++i) {
        arr.async_set(i, 2 * i);
      }
    }

    world.barrier();

    YGM_ASSERT_RELEASE(arr.size() == size_t(initial_size));

    arr.clear();

    YGM_ASSERT_RELEASE(arr.size() == 0);
  }

  // Test swap
  {
    int size1 = 32;
    int size2 = 48;

    ygm::container::array<int> arr1(world, size1);
    ygm::container::array<int> arr2(world, size2);

    if (world.rank0()) {
      for (int i = 0; i < size1; ++i) {
        arr1.async_set(i, 2 * i);
      }
      for (int i = 0; i < size2; ++i) {
        arr2.async_set(i, 3 * i + 1);
      }
    }

    world.barrier();

    YGM_ASSERT_RELEASE(arr1.size() == size_t(size1));
    YGM_ASSERT_RELEASE(arr2.size() == size_t(size2));

    arr1.for_all([](const auto &index, const auto &value) {
      YGM_ASSERT_RELEASE(size_t(value) == 2 * index);
    });

    arr2.for_all([](const auto &index, const auto &value) {
      YGM_ASSERT_RELEASE(size_t(value) == 3 * index + 1);
    });

    arr1.swap(arr2);

    YGM_ASSERT_RELEASE(arr1.size() == size_t(size2));
    YGM_ASSERT_RELEASE(arr2.size() == size_t(size1));

    arr1.for_all([](const auto &index, const auto &value) {
      YGM_ASSERT_RELEASE(size_t(value) == 3 * index + 1);
    });

    arr2.for_all([](const auto &index, const auto &value) {
      YGM_ASSERT_RELEASE(size_t(value) == 2 * index);
    });
  }

  // Test constructor with default value
  {
    int size          = 64;
    int default_value = 3;

    ygm::container::array<int> arr(world, size, default_value);

    if (world.rank0()) {
      for (int i = 0; i < size; ++i) {
        if (i % 2 == 0) {
          arr.async_set(i, 2 * i);
        }
      }
    }

    world.barrier();

    arr.for_all([&default_value](const auto &index, const auto &value) {
      if (index % 2 == 0) {
        YGM_ASSERT_RELEASE(size_t(value) == 2 * index);
      } else {
        YGM_ASSERT_RELEASE(value == default_value);
      }
    });
  }

  // Test constructor with initializer list of values
  {
    ygm::container::array<int> arr(world, {1, 3, 5, 7, 9, 11});

    arr.for_all([](const auto &index, const auto &value) {
      YGM_ASSERT_RELEASE(size_t(value) == 2 * index + 1);
    });
  }

  // Test constructor with initializer list of index, value pairs
  {
    ygm::container::array<int> arr(
        world,
        {std::make_pair(1, 2), std::make_pair(3, 6), std::make_pair(5, 10),
         std::make_pair(7, 14), std::make_pair(9, 18), std::make_pair(11, 22)});

    arr.for_all([](const auto &index, const auto &value) {
      if (index % 2 == 1) {
        YGM_ASSERT_RELEASE(size_t(value) == 2 * index);
      } else {
        YGM_ASSERT_RELEASE(value == 0);
      }
    });
  }

  // Test constructor from bag
  {
    ygm::container::bag<int> b(world);
    int                      bag_size = 10;
    if (world.rank0()) {
      for (int i = 0; i < bag_size; ++i) {
        b.async_insert(1);
      }
    }

    world.barrier();

    ygm::container::array<int> arr(world, b);

    arr.for_all([]([[maybe_unused]] const auto &index, const auto &value) {
      YGM_ASSERT_RELEASE(value == 1);
    });
  }

  // Test constructor from bag of tuples
  {
    ygm::container::bag<std::tuple<int, int>> b(world);
    int                                       bag_size = 10;
    if (world.rank0()) {
      for (int i = 0; i < bag_size; ++i) {
        b.async_insert(std::make_tuple(2 * i, i));
      }
    }

    world.barrier();

    ygm::container::array<int> arr(world, b);

    YGM_ASSERT_RELEASE(arr.size() == size_t(2 * bag_size - 1));
    arr.for_all([](const auto &index, const auto &value) {
      if (index % 2 == 0) {
        YGM_ASSERT_RELEASE(size_t(value) == index / 2);
      } else {
        YGM_ASSERT_RELEASE(value == 0);
      }
    });
  }

  // Test constructor from map
  {
    ygm::container::map<int, int> m(world);
    int                           bag_size = 10;
    if (world.rank0()) {
      for (int i = 0; i < bag_size; ++i) {
        m.async_insert(2 * i, i);
      }
    }

    world.barrier();

    ygm::container::array<int> arr(world, m);

    YGM_ASSERT_RELEASE(arr.size() == size_t(2 * bag_size - 1));
    arr.for_all([](const auto &index, const auto &value) {
      if (index % 2 == 0) {
        YGM_ASSERT_RELEASE(size_t(value) == index / 2);
      } else {
        YGM_ASSERT_RELEASE(value == 0);
      }
    });
  }

  // Test constructor from std::vector
  {
    std::vector<int> local_vec;
    int              start_index = world.rank() * (world.rank() + 1) / 2;
    for (int i = 0; i < world.rank() + 1; ++i) {
      local_vec.push_back(start_index++);
    }

    ygm::container::array<int> arr(world, local_vec);

    YGM_ASSERT_RELEASE(arr.size() ==
                       size_t(world.size()) * (world.size() + 1) / 2);
    arr.for_all([](const auto &index, const auto &value) {
      YGM_ASSERT_RELEASE(size_t(value) == index);
    });
  }

  // Test constructor from std::vector of tuples
  {
    std::vector<std::tuple<int, float>> local_vec;
    int                                 local_size = 10;

    for (int i = 0; i < local_size; ++i) {
      local_vec.push_back(std::make_tuple(world.size() * i + world.rank(),
                                          float(world.rank())));
    }

    ygm::container::array<float> arr(world, local_vec);

    YGM_ASSERT_RELEASE(arr.size() == size_t(world.size() * local_size));
    arr.for_all([&world](const auto &index, const auto &value) {
      YGM_ASSERT_RELEASE(value == float(index % world.size()));
    });
  }

  // Test constructor from std::map
  {
    std::map<int, float> local_map;
    int                  local_size = 10;

    for (int i = 0; i < local_size; ++i) {
      local_map[world.size() * i + world.rank()] = float(world.rank());
    }

    ygm::container::array<float> arr(world, local_map);

    YGM_ASSERT_RELEASE(arr.size() == size_t(world.size() * local_size));
    arr.for_all([&world](const auto &index, const auto &value) {
      YGM_ASSERT_RELEASE(value == float(index % world.size()));
    });
  }

  // Test sort
  {
    int                        num_values = 91;
    ygm::container::array<int> arr(world, num_values);

    if (world.rank0()) {
      std::vector<int> values;
      for (int i = 0; i < num_values; ++i) {
        values.push_back(i);
      }
      std::random_device rd;
      std::shuffle(values.begin(), values.end(), rd);

      int index{0};
      for (const auto v : values) {
        arr.async_insert(index++, v);
      }
    }

    world.barrier();

    arr.sort();

    arr.for_all([](const auto index, const auto &value) {
      YGM_ASSERT_RELEASE(index == size_t(value));
    });
  }

  return 0;
}
