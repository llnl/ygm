// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <tuple>
#include <utility>
#include <ygm/container/detail/base_concepts.hpp>
#include <ygm/detail/lambda_compliance.hpp>

namespace ygm::container::detail {

/**
 * @brief Curiously-recurring template parameter struct that provides
 * async_reduce operation
 */
template <typename derived_type, typename for_all_args>
struct base_async_reduce {
  /**
   * @brief Combines existing `mapped_type` item with `value` using a
   * user-provided binary operation if `key` is found in container. Inserts
   * default `mapped_type` prior to reduction if `key` does not already exist in
   * container.
   *
   * @tparam ReductionOp Type of function provided by usert to perform reduction
   * @param key Key to search for within container
   * @param value Provided value to combine with existing value in container
   *
   * \code{cpp}
   * ygm::container::map<std::string, int> my_map(world);
   * my_map.async_insert("one", 1);
   * if (world.rank0()) {
   *    my_map.async_reduce("one", 2, std::plus<int>());
   *    my_map.async_reduce("two", 2, std::plus<int>());
   * }
   * world.barrier()
   * \endcode
   * will result in `my_map` containing the pairs `("one", 3)` and `("two", 2)`.
   */
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
