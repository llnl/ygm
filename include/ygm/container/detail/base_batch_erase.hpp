// Copyright 2019-2021 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <tuple>
#include <utility>
#include <ygm/container/detail/base_concepts.hpp>

namespace ygm::container::detail {

template <typename derived_type, typename for_all_args>
struct base_batch_erase_key {
  using key_type = typename std::tuple_element_t<0, for_all_args>;

  template <typename Container>
  void erase(const Container &cont) requires detail::HasForAll<Container> &&
      SingleItemTuple<typename Container::for_all_args> && std::convertible_to<
          typename std::tuple_element_t<0, typename Container::for_all_args>,
          key_type> {
    derived_type *derived_this = static_cast<derived_type *>(this);

    cont.for_all(
        [derived_this](const auto &key) { derived_this->async_erase(key); });

    derived_this->comm().barrier();
  }

  template <typename Container>
  void erase(const Container &cont) requires STLContainer<Container> &&
      AtLeastOneItemTuple<for_all_args> &&
      std::convertible_to<typename Container::value_type, key_type> {
    derived_type *derived_this = static_cast<derived_type *>(this);

    for (const auto &key : cont) {
      derived_this->async_erase(key);
    }

    derived_this->comm().barrier();
  }
};

template <typename derived_type, typename for_all_args>
struct base_batch_erase_key_value {
  using key_type    = typename std::tuple_element_t<0, for_all_args>;
  using mapped_type = typename std::tuple_element_t<1, for_all_args>;

  template <typename Container>
  void erase(const Container &cont) requires HasForAll<Container> &&
      DoubleItemTuple<typename Container::for_all_args> && std::convertible_to<
          typename std::tuple_element_t<0, typename Container::for_all_args>,
          key_type> &&
      std::convertible_to<
          typename std::tuple_element_t<1, typename Container::for_all_args>,
          mapped_type> {
    derived_type *derived_this = static_cast<derived_type *>(this);

    cont.for_all([derived_this](const auto &key, const auto &value) {
      derived_this->async_erase(key, value);
    });

    derived_this->comm().barrier();
  }

  template <typename Container>
  void erase(const Container &cont) requires HasForAll<Container> &&
      SingleItemTuple<typename Container::for_all_args> && DoubleItemTuple<
          typename std::tuple_element_t<0, typename Container::for_all_args>> &&
      std::convertible_to<typename std::tuple_element_t<
                              0, typename std::tuple_element_t<
                                     0, typename Container::for_all_args>>,
                          key_type> &&
      std::convertible_to<typename std::tuple_element_t<
                              1, typename std::tuple_element_t<
                                     0, typename Container::for_all_args>>,
                          mapped_type> {
    derived_type *derived_this = static_cast<derived_type *>(this);

    cont.for_all([derived_this](const auto &key_value) {
      const auto &[key, value] = key_value;

      derived_this->async_erase(key, value);
    });

    derived_this->comm().barrier();
  }

  template <typename Container>
  void erase(const Container &cont) requires STLContainer<Container> &&
      DoubleItemTuple<typename Container::value_type> && std::convertible_to<
          typename std::tuple_element_t<0, typename Container::value_type>,
          key_type> &&
      std::convertible_to<
          typename std::tuple_element_t<1, typename Container::value_type>,
          mapped_type> {
    derived_type *derived_this = static_cast<derived_type *>(this);

    derived_this->comm().barrier();

    for (const auto &key_value : cont) {
      const auto &[key, value] = key_value;
      derived_this->async_erase(key, value);
    }

    derived_this->comm().barrier();
  }

  // Copies of base_batch_erase_key functions to allow deletions from keys alone
  template <typename Container>
  void erase(const Container &cont) requires detail::HasForAll<Container> &&
      SingleItemTuple<typename Container::for_all_args> && std::convertible_to<
          typename std::tuple_element_t<0, typename Container::for_all_args>,
          key_type> {
    derived_type *derived_this = static_cast<derived_type *>(this);

    cont.for_all(
        [derived_this](const auto &key) { derived_this->async_erase(key); });

    derived_this->comm().barrier();
  }

  template <typename Container>
  void erase(const Container &cont) requires STLContainer<Container> &&
      AtLeastOneItemTuple<for_all_args> &&
      std::convertible_to<typename Container::value_type, key_type> {
    derived_type *derived_this = static_cast<derived_type *>(this);

    for (const auto &key : cont) {
      derived_this->async_erase(key);
    }

    derived_this->comm().barrier();
  }
};

}  // namespace ygm::container::detail
