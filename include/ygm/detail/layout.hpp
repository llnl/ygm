// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <exception>
#include <iostream>
#include <sstream>

#include <ygm/detail/mpi.hpp>

namespace ygm {
namespace detail {

class layout {
 private:
  int m_comm_size;
  int m_comm_rank;
  int m_node_size;
  int m_node_id;
  int m_local_size;
  int m_local_id;

  std::vector<int> m_strided_ranks;
  std::vector<int> m_local_ranks;

  std::vector<int> m_rank_to_node;
  std::vector<int> m_rank_to_local;

 public:
  layout(MPI_Comm comm) {
    // global ranks
    YGM_ASSERT_MPI(MPI_Comm_size(comm, &m_comm_size));
    YGM_ASSERT_MPI(MPI_Comm_rank(comm, &m_comm_rank));

    // local ranks
    MPI_Comm comm_local;
    YGM_ASSERT_MPI(MPI_Comm_split_type(comm, MPI_COMM_TYPE_SHARED, m_comm_rank,
                                       MPI_INFO_NULL, &comm_local));
    YGM_ASSERT_MPI(MPI_Comm_size(comm_local, &m_local_size));
    YGM_ASSERT_MPI(MPI_Comm_rank(comm_local, &m_local_id));

    _mpi_allgather(m_comm_rank, m_local_ranks, m_local_size, comm_local);

    // node ranks
    MPI_Comm comm_node;
    YGM_ASSERT_MPI(MPI_Comm_split(comm, m_local_id, m_comm_rank, &comm_node));
    YGM_ASSERT_MPI(MPI_Comm_size(comm_node, &m_node_size));
    YGM_ASSERT_MPI(MPI_Comm_rank(comm_node, &m_node_id));

    _mpi_allgather(m_comm_rank, m_strided_ranks, m_node_size, comm_node);

    _mpi_allgather(m_local_id, m_rank_to_local, m_comm_size, comm);
    _mpi_allgather(m_node_id, m_rank_to_node, m_comm_size, comm);

    YGM_ASSERT_RELEASE(MPI_Comm_free(&comm_local) == MPI_SUCCESS);
    YGM_ASSERT_RELEASE(MPI_Comm_free(&comm_node) == MPI_SUCCESS);
  }

  layout(const layout &rhs)
      : m_comm_size(rhs.m_comm_size),
        m_comm_rank(rhs.m_comm_rank),
        m_node_size(rhs.m_node_size),
        m_node_id(rhs.m_node_id),
        m_local_size(rhs.m_local_size),
        m_local_id(rhs.m_local_id),
        m_strided_ranks(rhs.m_strided_ranks),
        m_local_ranks(rhs.m_local_ranks),
        m_rank_to_node(rhs.m_rank_to_node),
        m_rank_to_local(rhs.m_rank_to_local) {}

  layout() {}

  friend void swap(layout &lhs, layout &rhs) {
    std::swap(lhs.m_comm_size, rhs.m_comm_size);
    std::swap(lhs.m_comm_rank, rhs.m_comm_rank);
    std::swap(lhs.m_node_size, rhs.m_node_size);
    std::swap(lhs.m_node_id, rhs.m_node_id);
    std::swap(lhs.m_local_size, rhs.m_local_size);
    std::swap(lhs.m_local_id, rhs.m_local_id);
    std::swap(lhs.m_strided_ranks, rhs.m_strided_ranks);
    std::swap(lhs.m_local_ranks, rhs.m_local_ranks);
    std::swap(lhs.m_rank_to_node, rhs.m_rank_to_node);
    std::swap(lhs.m_rank_to_local, rhs.m_rank_to_local);
  }

  ~layout() {}

  //////////////////////////////////////////////////////////////////////////////
  // global layout info
  //////////////////////////////////////////////////////////////////////////////

  constexpr int size() const { return m_comm_size; }
  constexpr int rank() const { return m_comm_rank; }

  constexpr int node_size() const { return m_node_size; }
  constexpr int local_size() const { return m_local_size; }

  //////////////////////////////////////////////////////////////////////////////
  // global perspective rank layout lookups
  //////////////////////////////////////////////////////////////////////////////

  constexpr int node_id() const { return m_node_id; }
  inline int    node_id(const int rank) const {
    _check_world_rank(rank);
    return m_rank_to_node.at(rank);
  }

  constexpr int local_id() const { return m_local_id; }
  inline int    local_id(const int rank) const {
    _check_world_rank(rank);
    return m_rank_to_local.at(rank);
  }

  constexpr std::pair<int, int> rank_to_nl() const {
    return {m_node_id, m_local_id};
  }
  inline std::pair<int, int> rank_to_nl(const int rank) const {
    return {node_id(rank), local_id(rank)};
  }

  inline int nl_to_rank(const int nid, const int lid) const {
    _check_node_rank(nid);
    _check_local_rank(lid);
    return nid * m_local_size + lid;
  }
  inline int nl_to_rank(const std::pair<int, int> pid) const {
    return nl_to_rank(pid.first, pid.second);
  }

  //////////////////////////////////////////////////////////////////////////////
  // local perspective rank layout lookups
  //////////////////////////////////////////////////////////////////////////////

  inline bool is_strided(const int rank) const {
    _check_world_rank(rank);
    return m_local_id == local_id(rank);
  }
  inline bool is_local(const int rank) const {
    _check_world_rank(rank);
    return m_node_id == node_id(rank);
  }

  //////////////////////////////////////////////////////////////////////////////
  // cached local and strided ranks
  //////////////////////////////////////////////////////////////////////////////

  constexpr const std::vector<int> &strided_ranks() const {
    return m_strided_ranks;
  }
  constexpr const std::vector<int> &local_ranks() const {
    return m_local_ranks;
  }

 private:
  template <typename T>
  void _mpi_allgather(T &_t, std::vector<T> &out_vec, int size, MPI_Comm comm) {
    out_vec.resize(size);
    YGM_ASSERT_MPI(MPI_Allgather(&_t, sizeof(_t), MPI_BYTE, &(out_vec[0]),
                                 sizeof(_t), MPI_BYTE, comm));
  }

  inline void _check_world_rank(const int rank) const {
    _check_rank(rank, m_comm_size, "world");
  }
  inline void _check_local_rank(const int local_rank) const {
    _check_rank(local_rank, m_local_size, "local");
  }
  inline void _check_node_rank(const int node_rank) const {
    _check_rank(node_rank, m_node_size, "node");
  }
  inline void _check_rank(const int rank, const int size,
                          const char *scope) const {
    if (rank < 0 || rank > size) {
      std::stringstream ss;
      ss << scope << " rank " << rank << " is not in the range [0, " << size
         << "]";
      throw std::logic_error(ss.str());
    }
  }
};

}  // namespace detail
}  // namespace ygm
