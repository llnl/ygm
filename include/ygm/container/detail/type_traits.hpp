// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <functional>
#include <tuple>
#include <utility>

namespace ygm::container::detail::type_traits {
template <template <typename...> class T, typename U>
struct is_specialization_of : std::false_type {};

template <template <typename...> class T, typename... Us>
struct is_specialization_of<T, T<Us...>> : std::true_type {};

template <typename T>
struct is_vector
    : is_specialization_of<std::vector, typename std::decay<T>::type> {};

template <typename T>
struct is_tuple
    : is_specialization_of<std::tuple, typename std::decay<T>::type> {};

template <typename T>
struct is_pair : is_specialization_of<std::pair, typename std::decay<T>::type> {
};

template <class T, bool isTuple>
struct tuple_wrapper_helper  // T is not a tuple
{
  using type = std::tuple<T>;
};

template <class T>
struct tuple_wrapper_helper<T, true>  // T is a tuple
{
  using type = T;
};

template <class T>
struct tuple_wrapper  // T is a tuple
{
  using type = typename tuple_wrapper_helper<T, is_tuple<T>::value>::type;
};
}  // namespace ygm::container::detail::type_traits
