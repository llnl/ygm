// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <functional>
#include <tuple>
#include <utility>
#include <ygm/container/detail/type_traits.hpp>

namespace ygm::container::detail {

template <typename Container, typename MapFunction>
class transform_proxy_value
    : public base_iteration_value<
          transform_proxy_value<Container, MapFunction>,
          typename type_traits::tuple_wrapper<decltype(std::apply(
              std::declval<MapFunction>(),
              std::declval<typename Container::for_all_args>()))>::type> {
 private:
  using map_function_ret =
      decltype(std::apply(std::declval<MapFunction>(),
                          std::declval<typename Container::for_all_args>()));

 public:
  using for_all_args =
      typename type_traits::tuple_wrapper<map_function_ret>::type;

  transform_proxy_value(Container& rc, MapFunction filter)
      : m_rcontainer(rc), m_map_fn(filter) {}

  template <typename Function>
  void for_all(Function fn) {
    auto mlambda = [fn, this](auto&... xs) {
      auto map_result = m_map_fn(std::forward<decltype(xs)>(xs)...);
      if constexpr (type_traits::is_tuple<decltype(map_result)>::value) {
        std::apply(fn, map_result);
      } else {
        fn(map_result);
      }
    };

    m_rcontainer.for_all(mlambda);
  }

  template <typename Function>
  void for_all(Function fn) const {
    auto mlambda = [fn, this](const auto&... xs) {
      auto map_result = m_map_fn(std::forward<decltype(xs)>(xs)...);
      if constexpr (type_traits::is_tuple<decltype(map_result)>::value) {
        std::apply(fn, std::move(map_result));
      } else {
        fn(std::move(map_result));
      }
    };

    m_rcontainer.for_all(mlambda);
  }

  ygm::comm& comm() { return m_rcontainer.comm(); }

  const ygm::comm& comm() const { return m_rcontainer.comm(); }

 private:
  Container&  m_rcontainer;
  MapFunction m_map_fn;
};

template <typename Container, typename MapFunction>
class transform_proxy_key_value
    : public base_iteration_value<
          transform_proxy_key_value<Container, MapFunction>,
          typename type_traits::tuple_wrapper<decltype(std::apply(
              std::declval<MapFunction>(),
              std::declval<typename Container::for_all_args>()))>::type> {
 private:
  using map_function_ret =
      decltype(std::apply(std::declval<MapFunction>(),
                          std::declval<typename Container::for_all_args>()));

 public:
  using for_all_args =
      typename type_traits::tuple_wrapper<map_function_ret>::type;

  transform_proxy_key_value(Container& rc, MapFunction filter)
      : m_rcontainer(rc), m_map_fn(filter) {}

  template <typename Function>
  void for_all(Function fn) {
    auto mlambda = [fn, this](auto&... xs) {
      auto map_result = m_map_fn(std::forward<decltype(xs)>(xs)...);
      if constexpr (type_traits::is_tuple<decltype(map_result)>::value) {
        std::apply(fn, map_result);
      } else {
        fn(map_result);
      }
    };

    m_rcontainer.for_all(mlambda);
  }

  template <typename Function>
  void for_all(Function fn) const {
    auto mlambda = [fn, this](const auto&... xs) {
      auto map_result = m_map_fn(std::forward<decltype(xs)>(xs)...);
      if constexpr (type_traits::is_tuple<decltype(map_result)>::value) {
        std::apply(fn, std::move(map_result));
      } else {
        fn(std::move(map_result));
      }
    };

    m_rcontainer.for_all(mlambda);
  }

  ygm::comm& comm() { return m_rcontainer.comm(); }

  const ygm::comm& comm() const { return m_rcontainer.comm(); }

 private:
  Container&  m_rcontainer;
  MapFunction m_map_fn;
};

}  // namespace ygm::container::detail
