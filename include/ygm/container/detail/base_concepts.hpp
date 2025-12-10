// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <tuple>
#include <type_traits>

namespace ygm::container::detail {

template <typename T>
concept SingleItemTuple =
    requires(T v) { requires std::tuple_size<T>::value == 1; };

template <typename T>
concept DoubleItemTuple =
    requires(T v) { requires std::tuple_size<T>::value == 2; };

template <typename T>
concept AtLeastOneItemTuple =
    requires(T v) { requires std::tuple_size<T>::value >= 1; };

template <typename T>
concept HasForAll = requires(T v) { typename T::for_all_args; };

template <typename T>
concept HasAsyncReduceWithReductionOp = requires(T v) {
  {
    std::declval<T>().async_reduce(
        std::declval<typename T::key_type>(),
        std::declval<typename T::mapped_type>(),
        [](const typename T::mapped_type                  a,
           [[maybe_unused]] const typename T::mapped_type b) { return a; })
  } -> std::same_as<void>;
};

template <typename T>
concept HasAsyncReduceWithoutReductionOp = requires(T v) {
  {
    std::declval<T>().async_reduce(std::declval<typename T::key_type>(),
                                   std::declval<typename T::mapped_type>())
  } -> std::same_as<void>;
};

template <typename T>
concept HasAsyncReduce = requires(T v) {
  requires HasAsyncReduceWithReductionOp<T> or
               HasAsyncReduceWithoutReductionOp<T>;
};

// Copied solution for an STL container concept from
// https://stackoverflow.com/questions/60449592/how-do-you-define-a-c-concept-for-the-standard-library-containers
template <class ContainerType>
concept STLContainer = requires(ContainerType a, const ContainerType b) {
  requires std::regular<ContainerType>;
  requires std::swappable<ContainerType>;
  requires std::destructible<typename ContainerType::value_type>;
  requires std::same_as<typename ContainerType::reference,
                        typename ContainerType::value_type &>;
  requires std::same_as<typename ContainerType::const_reference,
                        const typename ContainerType::value_type &>;
  requires std::forward_iterator<typename ContainerType::iterator>;
  requires std::forward_iterator<typename ContainerType::const_iterator>;
  requires std::signed_integral<typename ContainerType::difference_type>;
  requires std::same_as<typename ContainerType::difference_type,
                        typename std::iterator_traits<
                            typename ContainerType::iterator>::difference_type>;
  requires std::same_as<
      typename ContainerType::difference_type,
      typename std::iterator_traits<
          typename ContainerType::const_iterator>::difference_type>;
  { a.begin() } -> std::same_as<typename ContainerType::iterator>;
  { a.end() } -> std::same_as<typename ContainerType::iterator>;
  { b.begin() } -> std::same_as<typename ContainerType::const_iterator>;
  { b.end() } -> std::same_as<typename ContainerType::const_iterator>;
  { a.cbegin() } -> std::same_as<typename ContainerType::const_iterator>;
  { a.cend() } -> std::same_as<typename ContainerType::const_iterator>;
  { a.size() } -> std::same_as<typename ContainerType::size_type>;
  { a.max_size() } -> std::same_as<typename ContainerType::size_type>;
  { a.empty() } -> std::same_as<bool>;
};
}  // namespace ygm::container::detail
