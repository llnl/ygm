// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <cereal/archives/json.hpp>
#include <cereal/types/utility.hpp>
#include <fstream>
#include <map>
#include <ygm/comm.hpp>
#include <ygm/detail/ygm_ptr.hpp>

#include <ygm/container/experimental/maptrix.hpp>
#include <ygm/container/map.hpp>

namespace ygm::container::experimental::detail::algorithms {

/* Operator times class. */
template <typename Value>
class times {
 public:
  Value operator()(const Value &a, const Value &b) const { return a * b; }
};

/* Function to support tranpose. */

template <typename Key, typename Value, typename OpPlus, typename OpMultiply>
ygm::container::map<Key, Value> spmv(
    ygm::container::experimental::maptrix<Key, Value> &A,
    ygm::container::map<Key, Value>                   &x,
    const OpPlus     &plus_op  = std::plus<Value>(),
    const OpMultiply &times_op = times<Value>()) {
  using key_type   = Key;
  using value_type = Value;
  using map_type   = ygm::container::map<key_type, value_type>;

  map_type y(A.comm());
  auto     y_ptr = y.get_ygm_ptr();

  auto kv_lambda = [&A, &y_ptr, &plus_op, &times_op](const auto &col,
                                                     const auto &col_value) {
    auto csc_visit_lambda = [](const auto &col, const auto &row,
                               const auto &A_value, const auto &x_value,
                               const auto &y_ptr, const auto &plus_op,
                               const auto &times_op) {
      auto element_wise = times_op(A_value, x_value);

      auto update_lambda = [](const auto &row_id, auto &row_val,
                              const auto &update_val, const auto &plus_op) {
        row_val = plus_op(row_val, update_val);
      };

      y_ptr->async_insert_if_missing_else_visit(row, element_wise,
                                                update_lambda, plus_op);
    };

    A.async_visit_col_const(col, csc_visit_lambda, col_value, y_ptr, plus_op,
                            times_op);
  };

  x.for_all(kv_lambda);
  A.comm().barrier();

  return y;
}

}  // namespace ygm::container::experimental::detail::algorithms
