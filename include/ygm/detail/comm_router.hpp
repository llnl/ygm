// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <ygm/detail/comm_environment.hpp>
#include <ygm/detail/layout.hpp>

namespace ygm {

namespace detail {

/**
 * @brief Provides routing destinations for messages in comm
 */
class comm_router {
 public:
  comm_router(const layout &l, const routing_type route = routing_type::NONE)
      : m_layout(l), m_default_route(route) {}

  /**
   * @brief Calculates the next hop based on the given routing scheme and final
   * destination
   *
   * @note The routes calculated should always satisfy the following
   * assumptions:
   * 1. routing_type::NONE sends directly to the destination
   * 2. routing_type::NR makes at most 2 hops, a remote hop followed by an
   * on-node hop
   * 3. routing_type::NLNR makes at most 3 hops, an on-node hop, followed by a
   * remote hop, followed by an on-node hop
   * 4. The pairs of remote processes communicating in routing_type::NLNR is a
   * subset of those communicating in routing_type::NR
   */
  int next_hop(const int dest, const routing_type route) const {
    int to_return;
    switch (route) {
      case routing_type::NONE:
        to_return = dest;
        break;
      case routing_type::NR:
        if (m_layout.is_local(dest)) {
          to_return = dest;
        } else {
          to_return = m_layout.strided_ranks()[m_layout.node_id(dest)];
        }
        break;
      case routing_type::NLNR:
        if (m_layout.is_local(dest)) {
          to_return = dest;
        } else {
          int dest_node = m_layout.node_id(dest);

          // Determine core offset for off-node communication
          int comm_channel_offset =
              (dest_node + m_layout.node_id()) % m_layout.local_size();
          int local_comm_rank = m_layout.local_ranks()[comm_channel_offset];

          if (m_layout.rank() == local_comm_rank) {
            to_return = m_layout.strided_ranks()[dest_node];
          } else {
            to_return = local_comm_rank;
          }
        }
        break;
      default:
        std::cerr << "Unknown routing type" << std::endl;
        return -1;
    }

    return to_return;
  }

  int next_hop(const int dest) const { return next_hop(dest, m_default_route); }

 private:
  const layout &m_layout;
  routing_type  m_default_route;
};

}  // namespace detail
}  // namespace ygm
