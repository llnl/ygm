// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

namespace ygm::container::detail {

template <typename Container>
class flatten_proxy_value
    : public base_iteration_value<flatten_proxy_value<Container>,
                                  std::tuple<typename std::tuple_element_t<
                                      0, typename Container::for_all_args>>> {
 public:
  using for_all_args = std::tuple<
      typename std::tuple_element_t<0, typename Container::for_all_args>>;

  flatten_proxy_value(Container& rc) : m_rcontainer(rc) {}

  template <typename Function>
  void for_all(Function fn) {
    auto flambda =
        [fn](typename std::tuple_element_t<0, typename Container::for_all_args>&
                 stlcont) {
          for (auto& v : stlcont) {
            fn(v);
          }
        };

    m_rcontainer.for_all(flambda);
  }

  template <typename Function>
  void for_all(Function fn) const {
    auto flambda =
        [fn](typename std::tuple_element_t<0, typename Container::for_all_args>&
                 stlcont) {
          for (const auto& v : stlcont) {
            fn(v);
          }
        };

    m_rcontainer.for_all(flambda);
  }

  ygm::comm& comm() { return m_rcontainer.comm(); }

  const ygm::comm& comm() const { return m_rcontainer.comm(); }

 private:
  Container& m_rcontainer;
};

template <typename Container>
class flatten_proxy_key_value : public base_iteration_key_value<
                                    flatten_proxy_value<Container>,
                                    std::tuple<typename std::tuple_element_t<
                                        0, typename Container::for_all_args>>> {
 public:
  using for_all_args = std::tuple<
      typename std::tuple_element_t<0, typename Container::for_all_args>>;

  flatten_proxy_key_value(Container& rc) : m_rcontainer(rc) {}

  template <typename Function>
  void for_all(Function fn) {
    auto flambda =
        [fn](typename std::tuple_element_t<0, typename Container::for_all_args>&
                 stlcont) {
          for (auto& v : stlcont) {
            fn(v);
          }
        };

    m_rcontainer.for_all(flambda);
  }

  template <typename Function>
  void for_all(Function fn) const {
    auto flambda =
        [fn](typename std::tuple_element_t<0, typename Container::for_all_args>&
                 stlcont) {
          for (const auto& v : stlcont) {
            fn(v);
          }
        };

    m_rcontainer.for_all(flambda);
  }

  ygm::comm& comm() { return m_rcontainer.comm(); }

  const ygm::comm& comm() const { return m_rcontainer.comm(); }

 private:
  Container& m_rcontainer;
};
}  // namespace ygm::container::detail
