// Copyright 2019-2021 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <tuple>
#include <utility>
#include <ygm/container/detail/base_concepts.hpp>
#include <ygm/detail/lambda_compliance.hpp>

namespace ygm::container::detail {

template <typename derived_type, typename for_all_args>
struct base_async_reduce {
  template <typename ReductionOp>
  void async_reduce(
      const typename std::tuple_element<0, for_all_args>::type& key,
      const typename std::tuple_element<1, for_all_args>::type& value,
      ReductionOp                                               reducer) {
    YGM_CHECK_ASYNC_LAMBDA_COMPLIANCE(ReductionOp,
                                      "ygm::container::async_reduce()");

    derived_type* derived_this = static_cast<derived_type*>(this);

    int dest = derived_this->partitioner.owner(key);

    auto rlambda =
        [reducer](auto pcont,
                  const typename std::tuple_element<0, for_all_args>::type& key,
                  const typename std::tuple_element<1, for_all_args>::type&
                      value) mutable {
          pcont->local_reduce(key, value, reducer);
        };

    derived_this->comm().async(dest, rlambda, derived_this->get_ygm_ptr(), key,
                               value);
  }
};

}  // namespace ygm::container::detail
