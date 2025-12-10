// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG

#include <map>
#include <set>
#include <vector>
#include <ygm/comm.hpp>
#include <ygm/container/bag.hpp>
#include <ygm/container/counting_set.hpp>
#include <ygm/container/map.hpp>
#include <ygm/detail/ygm_traits.hpp>

int main(int argc, char** argv) {
  ygm::comm world(&argc, &argv);

  auto l_int          = []([[maybe_unused]] int a) {};
  auto l_int_int      = []([[maybe_unused]] int a, [[maybe_unused]] int b) {};
  auto l_pair_int_int = []([[maybe_unused]] std::pair<int, int> p) {};

  //
  // vector<int>
  static_assert(ygm::detail::is_for_each_invocable<std::vector<int>,
                                                   decltype(l_int)>::value);
  static_assert(
      not ygm::detail::is_for_each_invocable<std::vector<int>,
                                             decltype(l_int_int)>::value);
  static_assert(
      not ygm::detail::is_for_each_invocable<std::vector<int>,
                                             decltype(l_pair_int_int)>::value);

  //
  // set<int>
  static_assert(ygm::detail::is_for_each_invocable<std::set<int>,
                                                   decltype(l_int)>::value);
  static_assert(
      not ygm::detail::is_for_each_invocable<std::set<int>,
                                             decltype(l_int_int)>::value);
  static_assert(
      not ygm::detail::is_for_each_invocable<std::set<int>,
                                             decltype(l_pair_int_int)>::value);

  //
  // map<int,int>
  static_assert(not ygm::detail::is_for_each_invocable<std::map<int, int>,
                                                       decltype(l_int)>::value);
  static_assert(
      not ygm::detail::is_for_each_invocable<std::map<int, int>,
                                             decltype(l_int_int)>::value);
  static_assert(
      ygm::detail::is_for_each_invocable<std::map<int, int>,
                                         decltype(l_pair_int_int)>::value);

  //
  // ygm::container::bag<int>
  static_assert(ygm::detail::is_for_all_invocable<ygm::container::bag<int>,
                                                  decltype(l_int)>::value);
  static_assert(
      not ygm::detail::is_for_all_invocable<ygm::container::bag<int>,
                                            decltype(l_int_int)>::value);
  static_assert(
      not ygm::detail::is_for_all_invocable<ygm::container::bag<int>,
                                            decltype(l_pair_int_int)>::value);

  // //
  // // ygm::container::map<int,int>
  //  TODO:   Needs for_all_args to work correctly, not value_type
  // static_assert(
  //     not ygm::detail::is_for_all_invocable<ygm::container::map<int, int>,
  //                                           decltype(l_int)>::value);
  // static_assert(ygm::detail::is_for_all_invocable<ygm::container::map<int,
  // int>,
  //                                                 decltype(l_int_int)>::value);
  // static_assert(
  //     not ygm::detail::is_for_all_invocable<ygm::container::map<int, int>,
  //                                           decltype(l_pair_int_int)>::value);

  return 0;
}
