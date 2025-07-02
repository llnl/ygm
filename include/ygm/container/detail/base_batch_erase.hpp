// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <tuple>
#include <utility>
#include <ygm/container/detail/base_concepts.hpp>

namespace ygm::container::detail {

/**
 * @brief Curiously-recurring template parameter struct that provides
 * erase operation that works from keys alone
 */
template <typename derived_type, typename for_all_args>
struct base_batch_erase_key {
  using key_type = typename std::tuple_element_t<0, for_all_args>;

  /**
   * @brief Erases keys contained in a `ygm::container` with a `for_all()`
   * method that are found in the calling container.
   *
   * @tparam Container YGM container type holding keys to erase
   * @param cont Container of keys
   * @details This variation requires the container of keys to have
   * `for_all_args` that is a tuple containing a single item.
   */
  template <typename Container>
  void erase(const Container &cont)
    requires detail::HasForAll<Container> &&
             SingleItemTuple<typename Container::for_all_args> &&
             std::convertible_to<typename std::tuple_element_t<
                                     0, typename Container::for_all_args>,
                                 key_type>
  {
    derived_type *derived_this = static_cast<derived_type *>(this);

    cont.for_all(
        [derived_this](const auto &key) { derived_this->async_erase(key); });

    derived_this->comm().barrier();
  }

  /**
   * @brief Erases keys contained in an STL container that are found in the
   * calling container.
   *
   * @tparam Container STL container type
   * @param cont STL container of keys to erase
   */
  template <typename Container>
  void erase(const Container &cont)
    requires STLContainer<Container> && AtLeastOneItemTuple<for_all_args> &&
             std::convertible_to<typename Container::value_type, key_type>
  {
    derived_type *derived_this = static_cast<derived_type *>(this);

    for (const auto &key : cont) {
      derived_this->async_erase(key);
    }

    derived_this->comm().barrier();
  }
};

/**
 * @brief Curiously-recurring template parameter struct that provides
 * erase operation that works from key-value pairs
 */
template <typename derived_type, typename for_all_args>
struct base_batch_erase_key_value {
  using key_type    = typename std::tuple_element_t<0, for_all_args>;
  using mapped_type = typename std::tuple_element_t<1, for_all_args>;

  /**
   * @brief Erases key-value pairs found in a `ygm::container` with a
   * `for_all()` method from the calling container
   *
   * @tparam Container YGM container type holding key-value pairs to erase
   * @param cont YGM container of key-value pairs to erase
   * @details This variation requires the container of key-value pairs to erase
   * to have a `for_all_args` that is a tuple containing two items.
   */
  template <typename Container>
  void erase(const Container &cont)
    requires HasForAll<Container> &&
             DoubleItemTuple<typename Container::for_all_args> &&
             std::convertible_to<typename std::tuple_element_t<
                                     0, typename Container::for_all_args>,
                                 key_type> &&
             std::convertible_to<typename std::tuple_element_t<
                                     1, typename Container::for_all_args>,
                                 mapped_type>
  {
    derived_type *derived_this = static_cast<derived_type *>(this);

    cont.for_all([derived_this](const auto &key, const auto &value) {
      derived_this->async_erase(key, value);
    });

    derived_this->comm().barrier();
  }

  /**
   * @brief Erases key-value pairs found in a `ygm::container` with a
   * `for_all()` method from the calling container
   *
   * @tparam Container YGM container type holding key-value pairs to erase
   * @param cont YGM container of key-value pairs to erase
   * @details This variation requires the container of key-value pairs to erase
   * to have a `for_all_args` that is a tuple containing a single item that is
   * itself a tuple of two items. This allows storing key-value pairs to erase
   * in a `ygm::container::bag`, for instance.
   */
  template <typename Container>
  void erase(const Container &cont)
    requires HasForAll<Container> &&
             SingleItemTuple<typename Container::for_all_args> &&
             DoubleItemTuple<typename std::tuple_element_t<
                 0, typename Container::for_all_args>> &&
             std::convertible_to<
                 typename std::tuple_element_t<
                     0, typename std::tuple_element_t<
                            0, typename Container::for_all_args>>,
                 key_type> &&
             std::convertible_to<
                 typename std::tuple_element_t<
                     1, typename std::tuple_element_t<
                            0, typename Container::for_all_args>>,
                 mapped_type>
  {
    derived_type *derived_this = static_cast<derived_type *>(this);

    cont.for_all([derived_this](const auto &key_value) {
      const auto &[key, value] = key_value;

      derived_this->async_erase(key, value);
    });

    derived_this->comm().barrier();
  }

  /**
   * @brief Erases key-value pairs found in an STL container from the calling
   * YGM container
   *
   * @tparam Container STL container type
   * @param cont STL container of key-value pairs to erase
   * @return This variant requires the STL container to have a `value_type` that
   * is a tuple containing key-value pairs.
   */
  template <typename Container>
  void erase(const Container &cont)
    requires STLContainer<Container> &&
             DoubleItemTuple<typename Container::value_type> &&
             std::convertible_to<typename std::tuple_element_t<
                                     0, typename Container::value_type>,
                                 key_type> &&
             std::convertible_to<typename std::tuple_element_t<
                                     1, typename Container::value_type>,
                                 mapped_type>
  {
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
  void erase(const Container &cont)
    requires detail::HasForAll<Container> &&
             SingleItemTuple<typename Container::for_all_args> &&
             std::convertible_to<typename std::tuple_element_t<
                                     0, typename Container::for_all_args>,
                                 key_type>
  {
    derived_type *derived_this = static_cast<derived_type *>(this);

    cont.for_all(
        [derived_this](const auto &key) { derived_this->async_erase(key); });

    derived_this->comm().barrier();
  }

  template <typename Container>
  void erase(const Container &cont)
    requires STLContainer<Container> && AtLeastOneItemTuple<for_all_args> &&
             std::convertible_to<typename Container::value_type, key_type>
  {
    derived_type *derived_this = static_cast<derived_type *>(this);

    for (const auto &key : cont) {
      derived_this->async_erase(key);
    }

    derived_this->comm().barrier();
  }
};

}  // namespace ygm::container::detail
