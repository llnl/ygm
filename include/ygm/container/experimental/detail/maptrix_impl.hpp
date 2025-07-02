// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <map>
#include <ygm/comm.hpp>
#include <ygm/detail/ygm_ptr.hpp>

#include <ygm/container/experimental/detail/column_view_impl.hpp>
#include <ygm/container/experimental/detail/row_view_impl.hpp>

namespace ygm::container::experimental::detail {

template <typename Key, typename Value,
          typename Partitioner =
              ygm::container::detail::old_hash_partitioner<Key>,
          typename Compare = std::less<Key>,
          class Alloc      = std::allocator<std::pair<const Key, Value>>>
class maptrix_impl {
 public:
  using key_type   = Key;
  using value_type = Value;
  using self_type  = maptrix_impl<Key, Value, Partitioner, Compare, Alloc>;

  using row_view_impl =
      detail::row_view_impl<key_type, value_type, Partitioner, Compare, Alloc>;
  using column_view_impl =
      detail::column_view_impl<key_type, value_type, Partitioner, Compare,
                               Alloc>;
  using map_type = ygm::container::map<key_type, value_type>;

  Partitioner partitioner;

  maptrix_impl(ygm::comm &comm)
      : m_row_view(comm),
        m_column_view(comm),
        m_comm(comm),
        pthis(this),
        m_default_value{} {
    m_comm.barrier();
  }

  maptrix_impl(ygm::comm &comm, const value_type &dv)
      : m_row_view(comm, dv),
        m_column_view(comm, dv),
        m_comm(comm),
        pthis(this),
        m_default_value(dv) {
    m_comm.barrier();
  }

  maptrix_impl(const self_type &rhs)
      : m_comm(rhs.m_comm), pthis(this), m_default_value(rhs.m_default_value) {
    m_comm.barrier();
    m_row_view.insert(std::begin(rhs.m_row_view), std::end(rhs.m_row_view));
    m_column_view.insert(std::begin(rhs.m_column_view),
                         std::end(rhs.m_column_view));
    m_comm.barrier();
  }

  ~maptrix_impl() { m_comm.barrier(); }

  void async_insert(const key_type &row, const key_type &col,
                    const value_type &value) {
    m_row_view.async_insert(row, col, value);
    m_column_view.async_insert(row, col, value);
  }

  ygm::comm &comm() { return m_comm; }

  /* For all is expected to be const on column_view. */
  template <typename Function>
  void for_all(Function fn) {
    m_column_view.for_all(fn);
  }

  template <typename Function>
  void for_all_row(Function fn) {
    m_row_view.for_all_row(fn);
  }

  template <typename Function>
  void for_all_col(Function fn) {
    m_column_view.for_all_col(fn);
  }

  template <typename... VisitorArgs>
  void print_all(std::ostream &os, VisitorArgs const &...args) {
    ((os << args), ...);
  }

  template <typename Visitor, typename... VisitorArgs>
  void async_visit_if_exists(const key_type &row, const key_type &col,
                             Visitor visitor, const VisitorArgs &...args) {
    m_row_view.async_visit_if_exists(row, col, visitor,
                                     std::forward<const VisitorArgs>(args)...);
    m_column_view.async_visit_if_exists(
        row, col, visitor, std::forward<const VisitorArgs>(args)...);
  }

  template <typename Visitor, typename... VisitorArgs>
  void async_visit_col_mutate(const key_type &col, Visitor visitor,
                              const VisitorArgs &...args) {
    auto &m_map     = m_column_view.column_view();
    auto &inner_map = m_map.find(col)->second;
    for (auto itr = inner_map.begin(); itr != inner_map.end(); ++itr) {
      key_type row = itr->first;
      m_row_view.async_visit_if_exists(
          row, col, visitor, std::forward<const VisitorArgs>(args)...);
      m_column_view.async_visit_if_exists(
          row, col, visitor, std::forward<const VisitorArgs>(args)...);
    }
  }

  template <typename Visitor, typename... VisitorArgs>
  void async_visit_row_const(const key_type &row, Visitor visitor,
                             const VisitorArgs &...args) {
    m_row_view.async_visit_row_const(row, visitor,
                                     std::forward<const VisitorArgs>(args)...);
  }

  template <typename Visitor, typename... VisitorArgs>
  void async_visit_col_const(const key_type &col, Visitor visitor,
                             const VisitorArgs &...args) {
    m_column_view.async_visit_col_const(
        col, visitor, std::forward<const VisitorArgs>(args)...);
  }

  template <typename Visitor, typename... VisitorArgs>
  void async_insert_if_missing_else_visit(const key_type   &row,
                                          const key_type   &col,
                                          const value_type &value,
                                          Visitor           visitor,
                                          const VisitorArgs &...args) {
    m_row_view.async_insert_if_missing_else_visit(
        row, col, value, visitor, std::forward<const VisitorArgs>(args)...);
    m_column_view.async_insert_if_missing_else_visit(
        row, col, value, visitor, std::forward<const VisitorArgs>(args)...);
  }

  typename ygm::ygm_ptr<self_type> get_ygm_ptr() const { return pthis; }

  void local_clear() {
    m_row_view.clear();
    m_column_view.clear();
  }

  void swap(self_type &s) {
    m_row_view.swap(s.row_view);
    m_column_view.swap(s.column_view);
  }

 protected:
  maptrix_impl() = delete;

  value_type                       m_default_value;
  row_view_impl                    m_row_view;
  column_view_impl                 m_column_view;
  ygm::comm                       &m_comm;
  typename ygm::ygm_ptr<self_type> pthis;
};
}  // namespace ygm::container::experimental::detail
