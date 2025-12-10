// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <ygm/comm.hpp>

#include <random>

namespace ygm::detail {

/// @brief Applies a simple offset to the specified seed according to rank index
/// @tparam ResultType The random number (seed) type; defaults to std::mt19937
/// @param comm The ygm::comm to be used
/// @param seed The specified seed
/// @return simply returns seed + rank
template <typename ResultType>
ResultType simple_offset(ygm::comm &comm, ResultType seed) {
  return seed + comm.rank();
}

/// @brief A wrapper around a per-rank random engine that manipulates each
///        rank's seed according to a specified strategy
/// @tparam RandomEngine The underlying random engine, e.g. std::mt19337
/// @tparam Function A `(ygm::comm, result_type) -> result_type` function that
///         modifies seeds for each rank
template <typename RandomEngine,
          typename RandomEngine::result_type (*Function)(
              ygm::comm &, typename RandomEngine::result_type)>
class random_engine {
 public:
  using rng_type    = RandomEngine;
  using result_type = typename RandomEngine::result_type;

  random_engine(ygm::comm &comm, result_type seed = std::random_device{}())
      : m_rng(Function(comm, seed)), m_seed(Function(comm, seed)) {}

  result_type operator()() { return m_rng(); }

  constexpr const result_type &seed() const { return m_seed; }

  static constexpr result_type min() { return rng_type::min(); }
  static constexpr result_type max() { return rng_type::max(); }

 private:
  rng_type    m_rng;
  result_type m_seed;
};
}  // namespace ygm::detail
