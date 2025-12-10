// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG
#include <ygm/comm.hpp>
#include <ygm/detail/ygm_ptr.hpp>

int main() {
  int provided;
  YGM_ASSERT_MPI(
      MPI_Init_thread(nullptr, nullptr, MPI_THREAD_MULTIPLE, &provided));
  YGM_ASSERT_RELEASE(MPI_THREAD_MULTIPLE == provided);

  for (size_t i = 0; i < 1000; ++i) {
    ygm::comm world(MPI_COMM_WORLD);
  }

  YGM_ASSERT_MPI(MPI_Finalize());
  return 0;
}
