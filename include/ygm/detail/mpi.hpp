// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <mpi.h>
#include <iostream>
#include <ygm/detail/ygm_traits.hpp>
#include <ygm/utility/assert.hpp>

namespace ygm::detail {
class mpi_init_finalize {
 public:
  mpi_init_finalize(int *argc, char ***argv) {
    YGM_ASSERT_MPI(MPI_Init(argc, argv));
  }
  ~mpi_init_finalize() {
    YGM_ASSERT_RELEASE(MPI_Barrier(MPI_COMM_WORLD) == MPI_SUCCESS);
    if (MPI_Finalize() != MPI_SUCCESS) {
      std::cerr << "ERROR:  MPI_Finalize() != MPI_SUCCESS" << std::endl;
      exit(-1);
    }
  }
};

template <typename T>
inline MPI_Datatype mpi_typeof(T) {
  static_assert(always_false<T>, "Unknown MPI Type");
  return 0;
}

template <>
inline MPI_Datatype mpi_typeof<char>(char) {
  return MPI_CHAR;
}

template <>
inline MPI_Datatype mpi_typeof<bool>(bool) {
  return MPI_CXX_BOOL;
}

inline MPI_Datatype mpi_typeof(std::signed_integral auto t) {
  if constexpr (sizeof(t) == 1) {
    return MPI_INT8_T;
  } else if constexpr (sizeof(t) == 2) {
    return MPI_INT16_T;
  } else if constexpr (sizeof(t) == 4) {
    return MPI_INT32_T;
  } else if constexpr (sizeof(t) == 8) {
    return MPI_INT64_T;
  } else {
    static_assert(always_false<decltype(t)>,
                  "Invalid integer size for MPI Type");
  }
}

inline MPI_Datatype mpi_typeof(std::unsigned_integral auto t) {
  if constexpr (sizeof(t) == 1) {
    return MPI_UINT8_T;
  } else if constexpr (sizeof(t) == 2) {
    return MPI_UINT16_T;
  } else if constexpr (sizeof(t) == 4) {
    return MPI_UINT32_T;
  } else if constexpr (sizeof(t) == 8) {
    return MPI_UINT64_T;
  } else {
    static_assert(always_false<decltype(t)>,
                  "Invalid integer size for MPI Type");
  }
}

template <>
inline MPI_Datatype mpi_typeof<float>(float) {
  return MPI_FLOAT;
}

template <>
inline MPI_Datatype mpi_typeof<double>(double) {
  return MPI_DOUBLE;
}

template <>
inline MPI_Datatype mpi_typeof<long double>(long double) {
  return MPI_LONG_DOUBLE;
}

}  // namespace ygm::detail
