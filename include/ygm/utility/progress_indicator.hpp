// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <mpi.h>
#include <ygm/comm.hpp>
#include <ygm/utility/assert.hpp>

namespace ygm::utility {

/**
 * @brief Simple progress indicator class
 *
 * \code{.cpp}
 *   ygm::progress_indicator prog(world);
 *   for (size_t i = 0; i < 1000; ++i) {
 *     prog.async_inc();
 *     std::this_thread::sleep_for(std::chrono::milliseconds(1));
 *   }
 *   prog.complete();
 *   world.barrier();
 * \endcode
 *
 */
class progress_indicator {
 public:
  /**
   * @brief
   *
   */
  struct options {
    /**
     * @brief How frequently to attempt global reduction
     */
    size_t update_freq = 10;
    /**
     * @brief Message header to print
     */
    std::string message = "Progress";
  };
  progress_indicator(ygm::comm& comm, const options& opts)
      : m_comm(comm), m_options(opts) {
    YGM_ASSERT_MPI(MPI_Comm_dup(comm.get_mpi_comm(), &m_mpi_comm));
    m_start_time = MPI_Wtime();
  }

  ~progress_indicator() { complete(); }

  /**
   * @brief Asynchronously update progress from local rank
   *
   * @param i amount to increment progress
   */
  void async_inc(size_t i = 1) {
    YGM_ASSERT_RELEASE(m_mpi_comm != MPI_COMM_NULL);
    m_local_count += i;
    if (m_local_last_report + m_options.update_freq <= m_local_count) {
      // Time to report, check m_mpi_iar_request first
      if (m_mpi_iar_request == MPI_REQUEST_NULL) {
        priv_post_iallreduce(false);
      } else {
        int flag = 0;
        YGM_ASSERT_MPI(MPI_Test(&m_mpi_iar_request, &flag, MPI_STATUS_IGNORE));
        if (flag) {
          YGM_ASSERT_RELEASE(m_mpi_iar_request == MPI_REQUEST_NULL);
          YGM_ASSERT_RELEASE(m_global_pair[1] < uint64_t(m_comm.size()));
          priv_report(false);
          priv_post_iallreduce(false);
        }
      }
    }
  }

  /**
   * @brief Complete the progress indicator
   * @details This is a collective function and should be called prior to a
   * barrier()
   */
  void complete() {
    if (m_mpi_comm == MPI_COMM_NULL) {
      return;
    }
    // Must wait until all ranks have finished.
    if (m_mpi_iar_request == MPI_REQUEST_NULL) {
      priv_post_iallreduce(true);
    }

    auto wait_until = [this]() -> bool {
      YGM_ASSERT_RELEASE(m_mpi_iar_request != MPI_REQUEST_NULL);
      int flag = 0;
      YGM_ASSERT_MPI(MPI_Test(&m_mpi_iar_request, &flag, MPI_STATUS_IGNORE));
      if (flag) {
        bool globally_complete = (m_global_pair[1] == uint64_t(m_comm.size()));
        if (!globally_complete) {
          priv_post_iallreduce(true);
        }
        priv_report(globally_complete);
        return globally_complete;
      }
      return false;
    };

    m_comm.local_wait_until(wait_until);

    YGM_ASSERT_RELEASE(MPI_Comm_free(&m_mpi_comm) == MPI_SUCCESS);
    YGM_ASSERT_RELEASE(m_mpi_comm == MPI_COMM_NULL);
  }

 private:
  void priv_post_iallreduce(bool in_complete) {
    YGM_ASSERT_RELEASE(m_mpi_iar_request == MPI_REQUEST_NULL);
    m_local_pair[0] = m_local_count;
    m_local_pair[1] = in_complete;

    YGM_ASSERT_MPI(MPI_Iallreduce(m_local_pair, m_global_pair, 2, MPI_UINT64_T,
                                  MPI_SUM, m_mpi_comm, &m_mpi_iar_request));
    m_local_last_report = m_local_count;
  }

  void priv_report(bool completed) {
    double rate = double(m_global_pair[0]) / (MPI_Wtime() - m_start_time);
    if (m_comm.rank0()) {
      std::cout << m_options.message << ": " << m_global_pair[0] << "\t\t"
                << std::fixed << std::setprecision(3) << rate << "\tper second";
      if (completed) {
        std::cout << std::endl;
      } else {
        std::cout << "\r" << std::flush;
      }
    }
  }

  MPI_Request m_mpi_iar_request = MPI_REQUEST_NULL;
  MPI_Comm    m_mpi_comm        = MPI_COMM_NULL;
  ygm::comm&  m_comm;
  options     m_options;
  size_t      m_local_count       = 0;
  size_t      m_local_last_report = 0;
  uint64_t    m_local_pair[2];
  uint64_t    m_global_pair[2];
  double      m_start_time;
};

}  // namespace ygm::utility
