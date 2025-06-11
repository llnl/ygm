// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once
#include <tuple>
#include <type_traits>

namespace ygm::detail {

// is_std_pair
template <typename>
struct is_std_pair_impl : std::false_type {};

template <typename... Ts>
struct is_std_pair_impl<std::pair<Ts...>> : std::true_type {};

template <typename T>
constexpr bool is_std_pair = is_std_pair_impl<T>::value;

}  // namespace ygm::detail
