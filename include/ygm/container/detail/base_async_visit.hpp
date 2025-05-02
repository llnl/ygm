// Copyright 2019-2021 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <tuple>
#include <utility>
#include <ygm/container/detail/base_concepts.hpp>
#include <ygm/detail/interrupt_mask.hpp>
#include <ygm/detail/lambda_compliance.hpp>

namespace ygm::container::detail {

template <typename derived_type, typename for_all_args>
struct base_async_visit {
  template <typename Visitor, typename... VisitorArgs>
  void async_visit(
      const typename std::tuple_element<0, for_all_args>::type& key,
      Visitor                                                   visitor,
      const VisitorArgs&... args) requires DoubleItemTuple<for_all_args> {
    YGM_CHECK_ASYNC_LAMBDA_COMPLIANCE(Visitor, "ygm::container::async_visit()");

    derived_type* derived_this = static_cast<derived_type*>(this);

    int dest = derived_this->partitioner.owner(key);

    auto vlambda =
        [visitor](auto pcont,
                  const typename std::tuple_element<0, for_all_args>::type& key,
                  const VisitorArgs&... args) mutable {
          pcont->local_visit(key, visitor, args...);
        };

    derived_this->comm().async(dest, vlambda, derived_this->get_ygm_ptr(), key,
                               args...);
  }

  template <typename Visitor, typename... VisitorArgs>
  void async_visit_if_contains(
      const typename std::tuple_element<0, for_all_args>::type& key,
      Visitor                                                   visitor,
      const VisitorArgs&... args) requires DoubleItemTuple<for_all_args> {
    YGM_CHECK_ASYNC_LAMBDA_COMPLIANCE(
        Visitor, "ygm::container::async_visit_if_contains()");

    derived_type* derived_this = static_cast<derived_type*>(this);

    int dest = derived_this->partitioner.owner(key);

    auto vlambda =
        [visitor](auto pcont,
                  const typename std::tuple_element<0, for_all_args>::type& key,
                  const VisitorArgs&... args) mutable {
          pcont->local_visit_if_contains(key, visitor, args...);
        };

    derived_this->comm().async(dest, vlambda, derived_this->get_ygm_ptr(), key,
                               args...);
  }

  template <typename Visitor, typename... VisitorArgs>
  void async_visit_if_contains(
      const typename std::tuple_element<0, for_all_args>::type& key,
      Visitor                                                   visitor,
      const VisitorArgs&... args) const requires DoubleItemTuple<for_all_args> {
    YGM_CHECK_ASYNC_LAMBDA_COMPLIANCE(
        Visitor, "ygm::container::async_visit_if_contains()");

    const derived_type* derived_this = static_cast<const derived_type*>(this);

    int dest = derived_this->partitioner.owner(key);

    auto vlambda =
        [visitor](const auto pcont,
                  const typename std::tuple_element<0, for_all_args>::type& key,
                  const VisitorArgs&... args) mutable {
          pcont->local_visit_if_contains(key, visitor, args...);
        };

    derived_this->comm().async(dest, vlambda, derived_this->get_ygm_ptr(), key,
                               args...);
  }

  // todo:   async_insert_visit()
};

}  // namespace ygm::container::detail
