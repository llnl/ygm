// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <ygm/detail/random.hpp>

namespace ygm {

/// @brief A simple offset rng alias
/// @tparam RandomEngine The underlying random engine, e.g. std::mt19937
template <typename RandomEngine = std::mt19937>
using default_random_engine =
    ygm::detail::random_engine<RandomEngine, ygm::detail::simple_offset>;

}  // namespace ygm