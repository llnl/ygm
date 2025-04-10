// Copyright 2019-2021 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <tuple>
#include <utility>
#include <ygm/detail/lambda_compliance.hpp>
#include <ygm/detail/meta/functional.hpp>

namespace ygm::container::detail {

template <typename derived_type, typename for_all_args>
struct base_async_contains {
  template <typename Function, typename... FuncArgs>
  void async_contains(
      const typename std::tuple_element<0, for_all_args>::type& value,
      Function fn, const FuncArgs&... args) {
    YGM_CHECK_ASYNC_LAMBDA_COMPLIANCE(Function,
                                      "ygm::container::async_contains()");

    derived_type* derived_this = static_cast<derived_type*>(this);

    int dest = derived_this->partitioner.owner(value);

    auto lambda =
        [fn](auto                                                      pcont,
             const typename std::tuple_element<0, for_all_args>::type& value,
             const FuncArgs&... args) mutable {
          bool contains = static_cast<bool>(pcont->local_count(value));
          ygm::meta::apply_optional(
              fn, std::make_tuple(pcont),
              std::forward_as_tuple(contains, value, args...));
        };

    derived_this->comm().async(dest, lambda, derived_this->get_ygm_ptr(), value,
                               args...);
  }
};

}  // namespace ygm::container::detail
