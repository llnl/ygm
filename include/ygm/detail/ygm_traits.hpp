// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once
#include <algorithm>
#include <type_traits>

namespace ygm::detail {

template <class...>
constexpr std::false_type always_false{};

namespace detector_detail {
// based on https://en.cppreference.com/w/cpp/experimental/is_detected
template <class Default, class AlwaysVoid, template <class...> class Op,
          class... Args>
struct detector {
  using value_t = std::false_type;
  using type    = Default;
};

template <class Default, template <class...> class Op, class... Args>
struct detector<Default, std::void_t<Op<Args...>>, Op, Args...> {
  using value_t = std::true_type;
  using type    = Op<Args...>;
};

struct nonesuch {
  nonesuch()                      = delete;
  ~nonesuch()                     = delete;
  nonesuch(nonesuch const&)       = delete;
  void operator=(nonesuch const&) = delete;
};

template <template <class...> class Op, class... Args>
using is_detected = typename detector<nonesuch, void, Op, Args...>::value_t;

template <typename T, typename... Ts>
using for_all_type = decltype(std::declval<T>().for_all(std::declval<Ts>()...));

template <typename T, typename... Ts>
using for_each_type = decltype(std::for_each(
    std::declval<T>().begin(), std::declval<T>().end(), std::declval<Ts>()...));

}  // namespace detector_detail

template <typename T>
using supports_for_all =
    detector_detail::is_detected<detector_detail::for_all_type, T, char>;

template <typename T>
using supports_for_each =
    detector_detail::is_detected<detector_detail::for_each_type, T, char>;

template <typename Container, typename Lambda>
using is_for_each_invocable =
    std::conjunction<supports_for_each<Container>,
                     std::is_invocable<Lambda, typename Container::value_type>>;

template <typename Container, typename Lambda>
using is_for_all_invocable =
    std::conjunction<supports_for_all<Container>,
                     std::is_invocable<Lambda, typename Container::value_type>>;

}  // namespace ygm::detail
