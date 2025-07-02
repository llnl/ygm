// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <ygm/detail/mpi.hpp>

namespace ygm {
/**
 * @brief Returns the MPI world rank
 *
 * @return int world rank
 */
inline int wrank() {
  int to_return;
  YGM_ASSERT_MPI(MPI_Comm_rank(MPI_COMM_WORLD, &to_return));
  return to_return;
}

/**
 * @brief Returns true if the calling rank is MPI world rank 0
 *
 * @return true Calling rank is MPI world rank 0
 * @return false Calling rank is not MPI world rank 0
 */
inline bool wrank0() { return wrank() == 0; }

/**
 * @brief Returns the MPI world size
 *
 * @return int world size
 */
inline int wsize() {
  int to_return;
  YGM_ASSERT_MPI(MPI_Comm_size(MPI_COMM_WORLD, &to_return));
  return to_return;
}

namespace detail {
/**
 * @brief Returns a dummy ostream to catch output that will not be printed to
 * cout or cerr
 */
inline std::ostream &dummy_ostream() {
  static std::ostringstream dummy;
  dummy.clear();
  return dummy;
}
}  // namespace detail

/**
 * @brief Returns a std::cout that only prints from MPI world rank 0
 *
 */
inline std::ostream &wcout0() {
  if (wrank0()) {
    return std::cout;
  }
  return detail::dummy_ostream();
}

/**
 * @brief Returns a std::cerr that only prints from MPI world rank 0
 *
 */
inline std::ostream &wcerr0() {
  if (wrank0()) {
    return std::cerr;
  }
  return detail::dummy_ostream();
}

/**
 * @brief Returns std::cout with MPI world rank prepended on the line
 *
 */
inline std::ostream &wcout() {
  std::cout << wrank() << ": ";
  return std::cout;
}

/**
 * @brief Returns std::cerr with MPI world rank prepended on the line
 *
 */
inline std::ostream &wcerr() {
  std::cerr << wrank() << ": ";
  return std::cerr;
}

namespace detail {
template <typename... Args>
inline std::string outstr0(Args &&...args) {
  std::stringstream ss;
  (ss << ... << args);
  return ss.str();
}

template <typename... Args>
inline std::string outstr(Args &&...args) {
  std::stringstream ss;
  ((ss << wrank() << ": ") << ... << args);
  return ss.str();
}
}  // namespace detail

/**
 * @brief Variadic function style print to std::cout with MPI world rank
 * prepended
 *
 */
template <typename... Args>
inline void wcout(Args &&...args) {
  std::cout << detail::outstr(args...) << std::endl;
}

/**
 * @brief Variadic function style print to std::cerr with MPI world rank
 * prepended
 *
 */
template <typename... Args>
inline void wcerr(Args &&...args) {
  std::cerr << detail::outstr(args...) << std::endl;
}

/**
 * @brief Variadic function style print to std::cout that only prints from MPI
 * rank 0.   All other ranks are ignored.
 *
 */
template <typename... Args>
inline void wcout0(Args &&...args) {
  if (wrank0()) {
    std::cout << detail::outstr0(args...) << std::endl;
  }
}

/**
 * @brief Variadic function style print to std::cerr that only prints from MPI
 * rank 0.   All other ranks are ignored.
 *
 */
template <typename... Args>
inline void wcerr0(Args &&...args) {
  if (wrank0()) {
    std::cerr << detail::outstr0(args...) << std::endl;
  }
}

}  // namespace ygm