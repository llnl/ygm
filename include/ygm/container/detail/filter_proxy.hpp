// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

namespace ygm::container::detail {

template <typename Container, typename FilterFunction>
class filter_proxy_value
    : public base_iteration_value<filter_proxy_value<Container, FilterFunction>,
                                  typename Container::for_all_args> {
 public:
  using for_all_args = typename Container::for_all_args;

  filter_proxy_value(Container& rc, FilterFunction filter)
      : m_rcontainer(rc), m_filter_fn(filter) {}

  template <typename Function>
  void for_all(Function fn) {
    auto flambda = [fn, this](auto&... xs) {
      bool b = m_filter_fn(std::forward<decltype(xs)>(xs)...);
      if (b) {
        fn(std::forward<decltype(xs)>(xs)...);
      }
    };

    m_rcontainer.for_all(flambda);
  }

  template <typename Function>
  void for_all(Function fn) const {
    auto flambda = [fn, this](const auto&... xs) {
      bool b = m_filter_fn(std::forward<decltype(xs)>(xs)...);
      if (b) {
        fn(std::forward<decltype(xs)>(xs)...);
      }
    };

    m_rcontainer.for_all(flambda);
  }

  ygm::comm& comm() { return m_rcontainer.comm(); }

  const ygm::comm& comm() const { return m_rcontainer.comm(); }

 private:
  Container&     m_rcontainer;
  FilterFunction m_filter_fn;
};

template <typename Container, typename FilterFunction>
class filter_proxy_key_value
    : public base_iteration_key_value<
          filter_proxy_key_value<Container, FilterFunction>,
          typename Container::for_all_args> {
 public:
  using for_all_args = typename Container::for_all_args;

  filter_proxy_key_value(Container& rc, FilterFunction filter)
      : m_rcontainer(rc), m_filter_fn(filter) {}

  template <typename Function>
  void for_all(Function fn) {
    auto flambda = [fn, this](auto&... xs) {
      bool b = m_filter_fn(std::forward<decltype(xs)>(xs)...);
      if (b) {
        fn(std::forward<decltype(xs)>(xs)...);
      }
    };

    m_rcontainer.for_all(flambda);
  }

  template <typename Function>
  void for_all(Function fn) const {
    auto flambda = [fn, this](const auto&... xs) {
      bool b = m_filter_fn(std::forward<decltype(xs)>(xs)...);
      if (b) {
        fn(std::forward<decltype(xs)>(xs)...);
      }
    };

    m_rcontainer.for_all(flambda);
  }

  ygm::comm& comm() { return m_rcontainer.comm(); }

  const ygm::comm& comm() const { return m_rcontainer.comm(); }

 private:
  Container&     m_rcontainer;
  FilterFunction m_filter_fn;
};

}  // namespace ygm::container::detail
