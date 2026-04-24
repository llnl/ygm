// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <set>
#include <string>

namespace ygm::detail {

/**
 * @brief Set of live ygm::comm UUIDs in the current process.
 *
 * @details Populated by ygm::comm::comm_setup() and drained by
 * ygm::comm::~comm().
 *
 * @note Can be removed if not required for downstream work.
 */
inline std::set<std::string> live_comm_uuids;

}  // namespace ygm::detail