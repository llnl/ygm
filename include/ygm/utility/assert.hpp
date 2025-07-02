// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once
#include <cassert>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace ygm::detail {
// work  on this:  https://github.com/lattera/glibc/blob/master/assert/assert.c
inline void release_assert_fail(const char *assertion, const char *file,
                                unsigned int line, const char *function) {
  std::stringstream ss;
  ss << " " << assertion << " " << file << ":" << line << " " << function
     << std::endl;
  throw std::runtime_error(ss.str());
}
}  // namespace ygm::detail

#define YGM_ASSERT_MPI(a)                                 \
  {                                                       \
    if (a != MPI_SUCCESS) {                               \
      char *error_string = NULL;                          \
      int   len          = 0;                             \
      MPI_Error_string(a, error_string, &len);            \
      std::stringstream ss;                               \
      ss << __FILE__ << ", line " << __LINE__             \
         << " MPI ERROR = " << error_string << std::endl; \
      throw std::runtime_error(ss.str());                 \
      exit(-1);                                           \
    }                                                     \
  }

#define YGM_ASSERT_DEBUG(expr) assert(expr)

#define YGM_ASSERT_RELEASE(expr) \
  (static_cast<bool>(expr)       \
       ? void(0)                 \
       : ygm::detail::release_assert_fail(#expr, __FILE__, __LINE__, ""))
