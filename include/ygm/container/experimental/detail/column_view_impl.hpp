// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <map>

#include <ygm/comm.hpp>
#include <ygm/detail/ygm_ptr.hpp>

#include <ygm/container/detail/hash_partitioner.hpp>
#include <ygm/container/map.hpp>

#include <ygm/container/experimental/detail/adj_impl.hpp>

namespace ygm::container::experimental::detail {

template <typename Key, typename Value,
          typename Partitioner =
              ygm::container::detail::old_hash_partitioner<Key>,
          typename Compare = std::less<Key>,
          class Alloc      = std::allocator<std::pair<const Key, Value>>>
class column_view_impl {
 public:
  using key_type       = Key;
  using value_type     = Value;
  using inner_map_type = std::map<Key, Value>;
  using self_type = column_view_impl<Key, Value, Partitioner, Compare, Alloc>;

  using map_type = ygm::container::map<key_type, value_type>;
  using adj_impl =
      detail::adj_impl<key_type, value_type, Partitioner, Compare, Alloc>;

  Partitioner partitioner;

  column_view_impl(ygm::comm &comm)
      : m_column_view(comm), m_comm(comm), pthis(this), m_default_value{} {
    m_comm.barrier();
  }

  column_view_impl(ygm::comm &comm, const value_type &dv)
      : m_column_view(comm), m_comm(comm), pthis(this), m_default_value(dv) {
    m_comm.barrier();
  }

  ~column_view_impl() { m_comm.barrier(); }

  void async_insert(const key_type &row, const key_type &col,
                    const value_type &value) {
    m_column_view.async_insert(col, row, value);
  }

  std::map<key_type, inner_map_type, Compare> &column_view() {
    return m_column_view.adj();
  }

  ygm::comm &comm() { return m_comm; }

  /* For all is expected to be const on column_view -> ensure
   * no lambda changes the value of the column_view elements...
   * how do you control parameters? */
  template <typename Function>
  void for_all(Function fn) {
    m_column_view.for_all(fn);
  }

  template <typename Function>
  void for_all_col(Function fn) {
    m_column_view.for_all_outer_key(fn);
  }

  template <typename... VisitorArgs>
  void print_all(std::ostream &os, VisitorArgs const &...args) {
    ((os << args), ...);
  }

  template <typename Visitor, typename... VisitorArgs>
  void async_visit_if_exists(const key_type &row, const key_type &col,
                             Visitor visitor, const VisitorArgs &...args) {
    m_column_view.async_visit_if_exists(
        col, row, visitor, std::forward<const VisitorArgs>(args)...);
  }

  template <typename Visitor, typename... VisitorArgs>
  void async_visit_col_mutate(const key_type &col, Visitor visitor,
                              const VisitorArgs &...args) {
    m_column_view.async_visit_mutate(col, visitor,
                                     std::forward<const VisitorArgs>(args)...);
  }

  template <typename Visitor, typename... VisitorArgs>
  void async_visit_col_const(const key_type &col, Visitor visitor,
                             const VisitorArgs &...args) {
    m_column_view.async_visit_const(col, visitor,
                                    std::forward<const VisitorArgs>(args)...);
  }

  template <typename Visitor, typename... VisitorArgs>
  void async_insert_if_missing_else_visit(const key_type   &row,
                                          const key_type   &col,
                                          const value_type &value,
                                          Visitor           visitor,
                                          const VisitorArgs &...args) {
    m_column_view.async_insert_if_missing_else_visit(
        col, row, value, visitor, std::forward<const VisitorArgs>(args)...);
  }

  typename ygm::ygm_ptr<self_type> get_ygm_ptr() const { return pthis; }

  void local_clear() { m_column_view.clear(); }

  void swap(self_type &s) { m_column_view.swap(s); }

 protected:
  column_view_impl() = delete;

  value_type                       m_default_value;
  adj_impl                         m_column_view;
  ygm::comm                       &m_comm;
  typename ygm::ygm_ptr<self_type> pthis;
};
}  // namespace ygm::container::experimental::detail
