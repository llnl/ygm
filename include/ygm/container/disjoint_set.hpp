// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <ygm/container/container_traits.hpp>
#include <ygm/container/detail/disjoint_set_impl.hpp>

namespace ygm::container {
template <typename Item,
          typename Partitioner = detail::old_hash_partitioner<Item>>
class disjoint_set {
 public:
  using self_type         = disjoint_set<Item, Partitioner>;
  using value_type        = Item;
  using size_type         = size_t;
  using ygm_for_all_types = std::tuple<Item, Item>;
  using container_type    = ygm::container::disjoint_set_tag;
  using impl_type         = detail::disjoint_set_impl<Item, Partitioner>;

  disjoint_set() = delete;

  disjoint_set(ygm::comm &comm, const size_t cache_size = 8192)
      : m_impl(comm, cache_size) {}

  template <typename Visitor, typename... VisitorArgs>
  void async_visit(const value_type &item, Visitor visitor,
                   const VisitorArgs &...args) {
    m_impl.async_visit(item, visitor, std::forward<const VisitorArgs>(args)...);
  }

  void async_union(const value_type &a, const value_type &b) {
    m_impl.async_union(a, b);
  }

  template <typename Function, typename... FunctionArgs>
  void async_union_and_execute(const value_type &a, const value_type &b,
                               Function fn, const FunctionArgs &...args) {
    m_impl.async_union_and_execute(a, b, fn,
                                   std::forward<const FunctionArgs>(args)...);
  }

  void all_compress() { m_impl.all_compress(); }

  template <typename Function>
  void for_all(Function fn) {
    m_impl.for_all(fn);
  }

  std::map<value_type, value_type> all_find(
      const std::vector<value_type> &items) {
    return m_impl.all_find(items);
  }

  void clear() { m_impl.clear(); }

  size_type size() { return m_impl.size(); }

  size_type num_sets() { return m_impl.num_sets(); }

  typename ygm::ygm_ptr<impl_type> get_ygm_ptr() const {
    return m_impl.get_ygm_ptr();
  }

  ygm::comm &comm() { return m_impl.comm(); }

 private:
  impl_type m_impl;
};
}  // namespace ygm::container
