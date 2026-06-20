#pragma once

#include <utility>

namespace ygm::graph::detail {

template <typename NodeLabel, typename EdgeWeight>
using edge_pack_type =
    std::pair<const std::pair<NodeLabel, NodeLabel>, EdgeWeight>;

template <typename NodeLabel, typename EdgeWeight>
edge_pack_type<NodeLabel, EdgeWeight> make_edge_pack(const NodeLabel&  s,
                                                     const NodeLabel&  t,
                                                     const EdgeWeight& w) {
  return std::make_pair(std::make_pair(s, t), w);
}

template <typename NodeLabel, typename EdgeWeight>
inline NodeLabel source(const edge_pack_type<NodeLabel, EdgeWeight>& pack) {
  return pack.first.first;
}

template <typename NodeLabel, typename EdgeWeight>
inline NodeLabel target(const edge_pack_type<NodeLabel, EdgeWeight>& pack) {
  return pack.first.second;
}

template <typename NodeLabel, typename EdgeWeight>
inline EdgeWeight weight(const edge_pack_type<NodeLabel, EdgeWeight>& pack) {
  return pack.second;
}

}  // namespace ygm::graph::detail