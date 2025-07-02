// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <tuple>
#include <utility>

// YGM_SKIP_ASYNC_LAMBDA_COMPLIANCE should not be used under normal
// circumstances but can be useful for testing, such as allowing functors with
// non-default constructors that print when called
#if YGM_SKIP_ASYNC_LAMBDA_COMPLIANCE
#define YGM_CHECK_ASYNC_LAMBDA_COMPLIANCE(func, location)
#else
#define YGM_CHECK_ASYNC_LAMBDA_COMPLIANCE(func, location)               \
  static_assert(                                                        \
      std::is_trivially_copyable<std::remove_reference<func>>::value && \
          std::is_standard_layout<std::remove_reference<func>>::value,  \
      location                                                          \
      " function object must be is_trivially_copyable & is_standard_layout.")
#endif
