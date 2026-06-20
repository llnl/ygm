// Copyright 2019-2026 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <variant>
#include <ygm/comm.hpp>
#include <ygm/container/counting_set.hpp>
#include <ygm/container/map.hpp>
#include <ygm/graph/detail/edge_pack.hpp>

namespace ygm::graph {

template <typename NodeLabel, typename EdgeWeight = std::monostate>
class directed_simple_edgelist {
 private:
  using storage_type =
      ygm::container::map<std::pair<NodeLabel, NodeLabel>, EdgeWeight>;

 public:
  using node_label_type  = NodeLabel;
  using edge_weight_type = EdgeWeight;
  using edge_pack_type   = detail::edge_pack_type<NodeLabel, EdgeWeight>;

  directed_simple_edgelist() = delete;

  directed_simple_edgelist(ygm::comm& comm) {}

  void async_insert(const node_label_type& u, const node_label_type& v,
                    const edge_weight_type& weight = edge_weight_type{})
    requires std::same_as<edge_weight_type, std::monostate>
  {
    if (u == v) {
      return;
    }
    m_emap.async_insert({u, v}, weight);
  }

  void async_insert_or_assign(
      const node_label_type& u, const node_label_type& v,
      const edge_weight_type& weight = edge_weight_type{}) {
    if (u == v) {
      return;
    }
    m_emap.async_insert_or_assign({u, v}, weight);
  }

  template <typename ReductionOp>
  void async_reduce(const node_label_type& u, const node_label_type& v,
                    const edge_weight_type& weight, ReductionOp reducer) {
    if (u == v) {
      return;
    }
    m_emap.async_reduce(std::make_pair(u, v), weight, reducer);
  }

  ygm::container::counting_set<node_label_type> count_out_degrees() const {
    ygm::container::counting_set<node_label_type> to_return(m_emap.comm());
    for (const edge_pack_type& ep : m_emap) {
      to_return.async_insert(source(ep));
    }
    return to_return;
  }

  ygm::container::counting_set<node_label_type> count_in_degrees() const {
    ygm::container::counting_set<node_label_type> to_return(m_emap.comm());
    for (const edge_pack_type& ep : m_emap) {
      to_return.async_insert(target(ep));
    }
    return to_return;
  }

  auto begin() { return m_emap.begin(); }

  auto begin() const { return m_emap.begin(); }

  auto cbegin() const { return m_emap.cbegin(); }

  auto end() { return m_emap.end(); }

  auto end() const { return m_emap.end(); }

  auto cend() const { return m_emap.cend(); }

  size_t size() const { return m_emap.size(); }

  void clear() { m_emap.clear(); }

  ygm::comm& comm() { return m_emap.comm(); }

 private:
  storage_type m_emap;
};

}  // namespace ygm::graph