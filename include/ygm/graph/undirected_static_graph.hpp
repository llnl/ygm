// Copyright 2019-2026 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <boost/unordered/unordered_flat_set.hpp>
#include <utility>
#include <variant>
#include <ygm/comm.hpp>
#include <ygm/container/array.hpp>
#include <ygm/container/bag.hpp>
#include <ygm/container/counting_set.hpp>
#include <ygm/detail/ygm_ptr.hpp>
#include <ygm/graph/detail/edge_pack.hpp>
#include <ygm/graph/undirected_multi_edgelist.hpp>
#include <ygm/graph/undirected_simple_edgelist.hpp>

namespace ygm::graph {

template <typename NodeLabel, typename EdgeWeight = std::monostate>
class undirected_static_graph {
  using self_type    = undirected_static_graph<NodeLabel, EdgeWeight>;
  using ygm_ptr_type = typename ygm::ygm_ptr<self_type>;

 public:
  enum class node_locator : size_t;
  using node_label_type = NodeLabel;
  using edge_data_type  = EdgeWeight;
  using label_edge_pack_type =
      detail::edge_pack_type<node_label_type, edge_data_type>;
  using locator_edge_pack_type =
      detail::edge_pack_type<node_locator, edge_data_type>;

  undirected_static_graph() = delete;

  undirected_static_graph(
      ygm::comm&                                               comm,
      const undirected_simple_edgelist<NodeLabel, EdgeWeight>& el)
      : m_comm(comm),
        pthis(this),
        m_multigraph(false),
        m_label_to_locator(comm),
        m_locator_to_label(comm) {
    pthis.check(m_comm);
    priv_ingest_edgelist(el);
  }

  undirected_static_graph(
      ygm::comm&                                              comm,
      const undirected_multi_edgelist<NodeLabel, EdgeWeight>& el)
      : m_comm(comm),
        pthis(this),
        m_multigraph(true),
        m_label_to_locator(comm),
        m_locator_to_label(comm) {
    pthis.check(m_comm);
    priv_ingest_edgelist(el);
  }

  // size_t num_nodes() const { return m_sorted_deg_nlb.size(); }

  // auto nodes_begin() const { return priv_node_range().begin(); }

  // void nodes_end() const { return priv_node_range().end(); }

  bool is_multigraph() const { return m_multigraph; }

 private:
  // auto priv_node_range() {
  //   m_nlb_to_nloc.comm().barrier();
  //   if (m_nlb_to_nloc.local_empty()) {
  //     return std::views::iota(size_t{0}, size_t{0}) |
  //            std::views::transform([](size_t i) { return node_locator{i}; });
  //   }
  //   size_t first = m_nlb_to_nloc.local_begin()->first;
  //   size_t dist =
  //     std::distance(m_nlb_to_nloc.local_begin(), m_nlb_to_nloc.local_end());
  //   return std::views::iota(first, first + dist) |
  //          std::views::transform([](size_t i) { return node_locator{i}; });
  // }

  template <typename EL>
  void priv_ingest_edgelist(const EL& el) {
    auto degree_counts = el.count_degrees();

    //
    // Build  m_label_to_locator & m_locator_to_label
    {
      // todo:  after ranges are supported skip the bag.
      ygm::container::bag<std::pair<size_t, NodeLabel>> bag_deg_node(m_comm);
      for (const auto& dc : degree_counts) {
        bag_deg_node.async_insert({dc.second, dc.first});
      }
      ygm::container::array<std::pair<size_t, NodeLabel>> sorted_deg_label(
          m_comm, bag_deg_node);
      bag_deg_node.clear();
      sorted_deg_label.sort();

      sorted_deg_label.for_all([&](size_t                              idx,
                                   const std::pair<size_t, NodeLabel>& p) {
        m_label_to_locator.async_insert_or_assign(p.second, node_locator{idx});
        m_locator_to_label.async_insert_or_assign(node_locator{idx}, p.second);
      });
    }

    //
    // Partition edges
    {
      static std::deque<locator_edge_pack_type>             slocal_edges;
      boost::unordered::unordered_flat_set<node_label_type> local_node_labels;
      for (const auto& e : el) {
        local_node_labels.insert(detail::source(e));
        local_node_labels.insert(detail::target(e));
      }
      auto local_label_to_locator = m_label_to_locator.template gather_keys<
          boost::unordered::unordered_flat_map<node_label_type, node_locator>>(
          local_node_labels);

      for (const auto& e : el) {
        node_locator unl   = local_label_to_locator.at(detail::source(e));
        node_locator vnl   = local_label_to_locator.at(detail::target(e));
        int          owner = priv_owner(unl, vnl);
        locator_edge_pack_type loce =
            detail::make_edge_pack(unl, vnl, detail::weight(e));
        m_comm.async(owner, [loce]() { slocal_edges.push_back(loce); });
      }
      m_comm.barrier();

      // build CSR

      slocal_edges.clear();
    }
  }

  int priv_owner(node_locator nl) {
    return std::to_underlying(nl) % m_comm.size();
  }

  int priv_owner(node_locator unl, node_locator vnl) {
    return std::to_underlying(std::min(unl, vnl)) % m_comm.size();
  }

  size_t priv_local(node_locator nl) {
    return std::to_underlying(nl) / m_comm.size();
  }

  ygm::comm&                                         m_comm;
  ygm_ptr_type                                       pthis;
  bool                                               m_multigraph;
  ygm::container::map<node_label_type, node_locator> m_label_to_locator;
  ygm::container::map<node_locator, node_label_type> m_locator_to_label;
};

}  // namespace ygm::graph
