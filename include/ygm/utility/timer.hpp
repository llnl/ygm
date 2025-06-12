// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <ygm/detail/mpi.hpp>

namespace ygm::utility {
class timer {
 public:
  timer() { reset(); }

  double elapsed() { return MPI_Wtime() - m_start; }

  void reset() { m_start = MPI_Wtime(); }

 private:
  double m_start;
};
}  // namespace ygm::utility