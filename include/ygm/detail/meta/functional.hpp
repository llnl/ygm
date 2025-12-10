// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <functional>
#include <tuple>
#include <type_traits>

namespace ygm {

namespace meta {
namespace details {
template <class Fn, class... Opts, class... Args>
auto apply(const std::true_type&, Fn&& fn, std::tuple<Opts...>&& optional,
           std::tuple<Args...>&& args) {
  using OptType = std::tuple<Opts...>;
  using ArgType = std::tuple<Args...>;

  return std::apply(std::forward<Fn>(fn),
                    std::tuple_cat(std::forward<OptType>(optional),
                                   std::forward<ArgType>(args)));
}

template <class Fn, class... Opts, class... Args>
auto apply(const std::false_type&, Fn&& fn,
           [[maybe_unused]] std::tuple<Opts...>&& optional,
           std::tuple<Args...>&&                  args) {
  using ArgType = std::tuple<Args...>;

  return std::apply(std::forward<Fn>(fn), std::forward<ArgType>(args));
}
}  // namespace details

/// calls fn(optional..., args...) if \ref fn expects optional... as the first
/// arguments
///       fn(args...)              otherwise
/// \tparam Fn     functor type
/// \tparam Opts   argument pack describing the tuple element types
/// \tparam Args   argument pack passed to the functor
/// \param  fn       the functor
/// \param  optional the optional arguments packed into a tuple
/// \param  args     the regular arguments packed into a tuple
/// \returns the result of calling \ref fn
template <class Fn, class... Opts, class... Args>
auto apply_optional(Fn&& fn, std::tuple<Opts...>&& optional,
                    std::tuple<Args...>&& args) {
  using tag     = typename std::is_invocable<Fn, Opts..., Args...>::type;
  using OptType = std::tuple<Opts...>;
  using ArgType = std::tuple<Args...>;

  return details::apply(tag{}, std::forward<Fn>(fn),
                        std::forward<OptType>(optional),
                        std::forward<ArgType>(args));
}
}  // namespace meta

}  // namespace ygm
