// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once
#include <boost/functional/hash.hpp>
#include <concepts>
#include <functional>

namespace ygm::container::detail {

template <typename T>
concept HasMethodHash = requires(const T &t) {
  { t.hash() } -> std::convertible_to<std::size_t>;
};

template <typename T>
concept HasStdHash = requires(const T &t) {
  { std::hash<T>{}(t) } -> std::convertible_to<std::size_t>;
};

template <typename T>
concept HasBoostHash = requires(const T &t) {
  { boost::hash<T>{}(t) } -> std::convertible_to<std::size_t>;
};

/**
 * @brief Default hash function for YGM
 *
 * Checks for available hash functions at compile time in this order: T.hash(),
 * std::hash<T>, then boost::hash<T>
 * @tparam T type to hash
 */
template <typename T>
struct hash {
  std::size_t operator()(const T &t) const {
    if constexpr (HasMethodHash<T>) {
      return t.hash();
    } else if constexpr (HasStdHash<T>) {
      return std::hash<T>{}(t);
    } else if constexpr (HasBoostHash<T>) {
      return boost::hash<T>{}(t);
    } else {
      static_assert(HasMethodHash<T> || HasStdHash<T> || HasBoostHash<T>,
                    "No suitable hash found for type T");
      return 0;
    }
  }
};

}  // namespace ygm::container::detail