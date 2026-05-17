// Copyright 2019-2026 Lawrence Livermore National Security, LLC and other YGM
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
 * @details Populated by ygm::comm::comm_setup() if stats_shm
 * enabled, and drained by ygm::comm::~comm().
 */
inline std::set<std::string> live_comm_uuids;

}  // namespace ygm::detail