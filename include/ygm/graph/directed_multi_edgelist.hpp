// Copyright 2019-2026 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <variant>
#include <ygm/comm.hpp>
#include <ygm/container/bag.hpp>
#include <ygm/container/counting_set.hpp>
#include <ygm/graph/detail/edge_pack.hpp>

namespace ygm::graph {

template <typename NodeLabel, typename EdgeWeight = std::monostate>
class directed_multi_edgelist {
 public:
  using node_label_type  = NodeLabel;
  using edge_weight_type = EdgeWeight;
  using edge_pack_type   = detail::edge_pack_type<NodeLabel, EdgeWeight>;

 private:
  using storage_type = ygm::container::bag<edge_pack_type>;

 public:
  directed_multi_edgelist() = delete;

  directed_multi_edgelist(ygm::comm& comm) : m_ebag(comm) {}

  void async_insert(const node_label_type& u, const node_label_type& v,
                    const edge_weight_type& weight = edge_weight_type{}) {
    m_ebag.async_visit(std::make_pair(std::make_pair(u, v), weight));
  }

  ygm::container::counting_set<node_label_type> count_out_degrees() const {
    ygm::container::counting_set<node_label_type> to_return(m_ebag.comm());
    for (const edge_pack_type& ep : m_ebag) {
      to_return.async_insert(source(ep));
    }
    return to_return;
  }

  ygm::container::counting_set<node_label_type> count_in_degrees() const {
    ygm::container::counting_set<node_label_type> to_return(m_ebag.comm());
    for (const edge_pack_type& ep : m_ebag) {
      to_return.async_insert(target(ep));
    }
    return to_return;
  }

  auto begin() { return m_ebag.begin(); }

  auto begin() const { return m_ebag.begin(); }

  auto cbegin() const { return m_ebag.cbegin(); }

  auto end() { return m_ebag.end(); }

  auto end() const { return m_ebag.end(); }

  auto cend() const { return m_ebag.cend(); }

  size_t size() const { return m_ebag.size(); }

  void clear() { m_ebag.clear(); }

  ygm::comm& comm() { return m_ebag.comm(); }

 private:
  storage_type m_ebag;
};

}  // namespace ygm::graph