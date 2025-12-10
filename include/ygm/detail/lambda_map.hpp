// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <limits>
#include <vector>
#include <ygm/detail/mpi.hpp>

namespace ygm {
namespace detail {

template <typename CFuncPtr, typename FuncId>
class lambda_map {
  template <typename LambdaType>
  struct lambda_enumerator {
    const static FuncId id;
  };

 public:
  using func_id = FuncId;

  template <typename LambdaType>
  static FuncId register_lambda([[maybe_unused]] LambdaType l) {
    return lambda_enumerator<LambdaType>::id;
  }
  template <typename... Args>
  void execute(FuncId id, const Args... args) {
    s_map[id](args...);
  }

 private:
  template <typename LambdaType>
  static FuncId record() {
    YGM_ASSERT_RELEASE(s_map.size() < std::numeric_limits<FuncId>::max());
    FuncId     to_return = s_map.size();
    LambdaType lp;  // can default construct lambdas that do not capture
    s_map.push_back(lp);
    return to_return;
  }
  static std::vector<CFuncPtr> s_map;
};
template <typename CFuncPtr, typename FuncId>
std::vector<CFuncPtr> lambda_map<CFuncPtr, FuncId>::s_map;

template <typename CFuncPtr, typename FuncId>
template <typename LambdaType>
const FuncId lambda_map<CFuncPtr, FuncId>::lambda_enumerator<LambdaType>::id =
    lambda_map<CFuncPtr, FuncId>::record<LambdaType>();

}  // namespace detail
}  // namespace ygm
