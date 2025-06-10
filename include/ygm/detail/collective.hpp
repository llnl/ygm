// Copyright 2019-2021 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <ygm/comm.hpp>

namespace ygm {

/**
 * @brief Collective computes the prefix sum of value across all ranks in the
 * communicator.
 *
 * @tparam T
 * @param value
 * @param c
 * @return T
 */
template <typename T>
T prefix_sum(const T &value, const comm &c) {
  T to_return{0};
  c.barrier();
  MPI_Comm mpi_comm = c.get_mpi_comm();
  YGM_ASSERT_MPI(MPI_Exscan(&value, &to_return, 1, detail::mpi_typeof(value),
                            MPI_SUM, mpi_comm));
  return to_return;
}

/**
 * @brief Collective computes the sum of value across all ranks in the
 * communicator.
 *
 * @tparam T
 * @param value
 * @param c
 * @return T
 */
template <typename T>
T sum(const T &value, const comm &c) {
  T to_return;
  c.barrier();
  MPI_Comm mpi_comm = c.get_mpi_comm();
  YGM_ASSERT_MPI(MPI_Allreduce(&value, &to_return, 1, detail::mpi_typeof(T()),
                               MPI_SUM, mpi_comm));
  return to_return;
}

/**
 * @brief Collective computes the min of value across all ranks in the
 * communicator.
 *
 * @tparam T
 * @param value
 * @param c
 * @return T
 */
template <typename T>
T min(const T &value, const comm &c) {
  T to_return;
  c.barrier();
  MPI_Comm mpi_comm = c.get_mpi_comm();
  YGM_ASSERT_MPI(MPI_Allreduce(&value, &to_return, 1, detail::mpi_typeof(T()),
                               MPI_MIN, mpi_comm));
  return to_return;
}

/**
 * @brief Collective computes the max of value across all ranks in the
 * communicator.
 *
 * @tparam T
 * @param value
 * @param c
 * @return T
 */
template <typename T>
T max(const T &value, const comm &c) {
  T to_return;
  c.barrier();
  MPI_Comm mpi_comm = c.get_mpi_comm();
  YGM_ASSERT_MPI(MPI_Allreduce(&value, &to_return, 1, detail::mpi_typeof(T()),
                               MPI_MAX, mpi_comm));
  return to_return;
}

/**
 * @brief Collective computes the logical and of value across all ranks in the
 * communicator.
 *
 * @tparam T
 * @param value
 * @param c
 * @return T
 */
inline bool logical_and(bool value, const comm &c) {
  bool to_return;
  c.barrier();
  MPI_Comm mpi_comm = c.get_mpi_comm();
  YGM_ASSERT_MPI(MPI_Allreduce(&value, &to_return, 1,
                               detail::mpi_typeof(bool()), MPI_LAND, mpi_comm));
  return to_return;
}

/**
 * @brief Collective computes the logical or of value across all ranks in the
 * communicator.
 *
 * @tparam T
 * @param value
 * @param c
 * @return T
 */
inline bool logical_or(bool value, const comm &c) {
  bool to_return;
  c.barrier();
  MPI_Comm mpi_comm = c.get_mpi_comm();
  YGM_ASSERT_MPI(MPI_Allreduce(&value, &to_return, 1,
                               detail::mpi_typeof(bool()), MPI_LOR, mpi_comm));
  return to_return;
}

/**
 * @brief Broadcasts to_bcast from root to all other ranks in communicator.
 *
 * @tparam T
 * @param to_bcast
 * @param root
 * @param cm
 */
template <typename T>
void bcast(T &to_bcast, int root, const comm &cm) {
  if constexpr (std::is_trivially_copyable<T>::value &&
                std::is_standard_layout<T>::value) {
    YGM_ASSERT_MPI(
        MPI_Bcast(&to_bcast, sizeof(T), MPI_BYTE, root, cm.get_mpi_comm()));
  } else {
    ygm::detail::byte_vector packed;
    cereal::YGMOutputArchive oarchive(packed);
    if (cm.rank() == root) {
      oarchive(to_bcast);
    }
    size_t packed_size = packed.size();
    YGM_ASSERT_RELEASE(packed_size < 1024 * 1024 * 1024);
    YGM_ASSERT_MPI(MPI_Bcast(&packed_size, 1,
                             ygm::detail::mpi_typeof(packed_size), root,
                             cm.get_mpi_comm()));
    if (cm.rank() != root) {
      packed.resize(packed_size);
    }
    YGM_ASSERT_MPI(MPI_Bcast(packed.data(), packed_size, MPI_BYTE, root,
                             cm.get_mpi_comm()));

    if (cm.rank() != root) {
      cereal::YGMInputArchive iarchive(packed.data(), packed.size());
      iarchive(to_bcast);
    }
  }
}

template <typename T, typename Equal = std::equal_to<T>>
bool is_same(const T &to_check, const comm &cm, const Equal &equals = Equal()) {
  T to_bcast;
  if (cm.rank() == 0) {
    to_bcast = to_check;
  }
  bcast(to_bcast, 0, cm);
  bool local_is_same = equals(to_check, to_bcast);
  return logical_and(local_is_same, cm);
}

/**
 * @brief Tree based reduction, could be optimized significantly
 *
 * @tparam T
 * @tparam MergeFunction
 * @param in
 * @param merge
 * @return T
 */
template <typename T, typename MergeFunction>
inline T all_reduce(const T &in, MergeFunction merge, const comm &cm) {
  int first_child  = 2 * cm.rank() + 1;
  int second_child = 2 * (cm.rank() + 1);
  int parent       = (cm.rank() - 1) / 2;

  // Step 1: Receive from children, merge into tmp
  T tmp = in;
  if (first_child < cm.size()) {
    T fc = cm.mpi_recv<T>(first_child, 0);
    tmp  = merge(tmp, fc);
  }
  if (second_child < cm.size()) {
    T sc = cm.mpi_recv<T>(second_child, 0);
    tmp  = merge(tmp, sc);
  }

  // Step 2: Send merged to parent
  if (cm.rank() != 0) {
    cm.mpi_send(tmp, parent, 0);
  }

  // Step 3:  Rank 0 bcasts
  T to_return = cm.mpi_bcast(tmp, 0);
  return to_return;
}

/**
 * @brief Tree based reduction, could be optimized significantly
 *
 * @tparam T
 * @tparam MergeFunction
 * @param in
 * @param merge
 * @return T
 */
template <typename T, typename MergeFunction>
inline std::optional<T> all_reduce(std::optional<T> mine, MergeFunction merge,
                                   const comm &cm) {
  int first_child  = 2 * cm.rank() + 1;
  int second_child = 2 * (cm.rank() + 1);
  int parent       = (cm.rank() - 1) / 2;

  // Step 1: Receive from children, merge into tmp
  if (first_child < cm.size()) {
    std::optional<T> fc = cm.mpi_recv<std::optional<T>>(first_child, 0);
    if (mine.has_value() && fc.has_value()) {
      mine = merge(mine.value(), fc.value());
    } else if (fc.has_value()) {
      mine = fc;
    }
  }
  if (second_child < cm.size()) {
    std::optional<T> sc = cm.mpi_recv<std::optional<T>>(second_child, 0);
    if (mine.has_value() && sc.has_value()) {
      mine = merge(mine.value(), sc.value());
    } else if (sc.has_value()) {
      mine = sc;
    }
  }

  // Step 2: Send merged to parent
  if (cm.rank() != 0) {
    cm.mpi_send(mine, parent, 0);
  }

  // Step 3:  Rank 0 bcasts
  return cm.mpi_bcast(mine, 0);
}

}  // namespace ygm
