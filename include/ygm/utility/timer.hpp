// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <ygm/detail/mpi.hpp>

namespace ygm::utility {

/**
 * @brief Simple timer class using `MPI_Wtime()`
 */
class timer {
 public:
  timer() { reset(); }

  /**
   * @brief Get time since timer creation or last `reset()`
   *
   * @return Elapsed time
   */
  double elapsed() { return MPI_Wtime() - m_start; }

  /**
   * @brief Restart timer
   */
  void reset() { m_start = MPI_Wtime(); }

 private:
  double m_start;
};
}  // namespace ygm::utility
