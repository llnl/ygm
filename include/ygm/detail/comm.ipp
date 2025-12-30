// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once
#include <ygm/detail/collective.hpp>
#include <ygm/detail/lambda_compliance.hpp>
#include <ygm/detail/meta/functional.hpp>
#include <ygm/detail/ygm_cereal_archive.hpp>
#include <ygm/detail/ygm_ptr.hpp>
#include <ygm/version.hpp>

namespace ygm {

struct comm::header_t {
  uint32_t message_size;
  int32_t  dest;
};

/**
 * @brief YGM communicator constructor
 *
 * @param argc Pointer to number of arguments given to command line
 * @param argv Pointer to array of command line arguments
 * @return Constructed ygm::comm object using MPI_COMM_WORLD for communication
 *
 * \code{cpp}
 * #include <ygm/comm.hpp>
 *
 * int main(int argc, char **argv) {
 *    ygm::comm world(&argc, &argv);
 * }
 * \endcode
 */
inline comm::comm(int *argc, char ***argv)
    : pimpl_if(std::make_shared<detail::mpi_init_finalize>(argc, argv)),
      m_layout(MPI_COMM_WORLD),
      m_router(m_layout, config.routing),
      m_logger(m_layout.rank()) {
  // pimpl_if = std::make_shared<detail::mpi_init_finalize>(argc, argv);
  comm_setup(MPI_COMM_WORLD);
}

/**
 * @brief YGM communicator constructor
 *
 * @param mcomm MPI communicator to use for underlying communication
 * @return Constructed ygm::comm object
 */
inline comm::comm(MPI_Comm mcomm)
    : m_layout(mcomm),
      m_router(m_layout, config.routing),
      m_logger(m_layout.rank()) {
  pimpl_if.reset();
  int flag(0);
  YGM_ASSERT_MPI(MPI_Initialized(&flag));
  if (!flag) {
    throw std::runtime_error("YGM::COMM ERROR: MPI not initialized");
  }
  comm_setup(mcomm);
}

/**
 * @brief Initialize ygm::comm internals
 *
 * @param c MPI communicator being used by ygm::comm
 * @details Makes necessary copies of MPI communicator for use by YGM, sets up
 * send buffers, and posts initial receives.
 */
inline void comm::comm_setup(MPI_Comm c) {
  m_logger.set_path(config.default_log_path + std::to_string(rank()));
  m_logger.set_log_level(config.default_log_level);
  m_logger.log(log_level::info, "Setting up ygm::comm");

  YGM_ASSERT_MPI(MPI_Comm_dup(c, &m_comm_async));
  YGM_ASSERT_MPI(MPI_Comm_dup(c, &m_comm_barrier));
  YGM_ASSERT_MPI(MPI_Comm_dup(c, &m_comm_other));

  m_vec_send_buffers.resize(m_layout.size());

  if (config.welcome) {
    welcome(std::cout);
  }

  for (size_t i = 0; i < config.num_irecvs; ++i) {
    std::shared_ptr<ygm::detail::byte_vector> recv_buffer{
        new ygm::detail::byte_vector(config.irecv_size)};
    post_new_irecv(recv_buffer);
  }

  if (m_trace_ygm || m_trace_mpi) {
    if (rank0()) m_tracer.create_directory();
    cf_barrier();
    m_tracer.open_file();
  }
}

/**
 * @brief Prints a YGM welcome statement including information about internal
 * YGM parameters.
 *
 * @param os Output stream to print welcome message to
 */
inline void comm::welcome(std::ostream &os) {
  static bool already_printed = false;
  if (already_printed) return;
  already_printed = true;
  std::stringstream sstr;
  sstr << "======================================\n"
       << " YY    YY     GGGGGG      MM     MM   \n"
       << "  YY  YY     GG    GG     MMM   MMM   \n"
       << "   YYYY      GG           MMMM MMMM   \n"
       << "    YY       GG   GGGG    MM MMM MM   \n"
       << "    YY       GG    GG     MM     MM   \n"
       << "    YY       GG    GG     MM     MM   \n"
       << "    YY        GGGGGG      MM     MM   \n"
       << "======================================\n"
       << "COMM_SIZE      = " << m_layout.size() << "\n"
       << "RANKS_PER_NODE = " << m_layout.local_size() << "\n"
       << "NUM_NODES      = " << m_layout.node_size() << "\n";

  // Find MPI implementation details
  char version[MPI_MAX_LIBRARY_VERSION_STRING];
  int  version_len;
  MPI_Get_library_version(version, &version_len);

  // Trim MPI details to implementation and version
  std::string version_string(version, version_len);
  std::string delimiters{',', '\n'};
  auto        end = version_string.find_first_of(delimiters);

  sstr << "MPI_LIBRARY    = " << version_string.substr(0, end) << "\n";
  sstr << "YGM_VERSION    = " << ygm_version << "\n";

  config.print(sstr);

  if (rank() == 0) {
    os << sstr.str();
  }
}

/**
 * @brief Resets counters within the comm_stats object being used by the
 * ygm::comm
 *
 * @details Useful for separating information about communication performed in
 * computation of interest from set-up or from other trials of the same
 * experiment.
 */
inline void comm::stats_reset() { m_stats.reset(); }

/**
 * @brief Prints information about communication tracked in comm_stats object
 *
 * @param name Label to be printed with stats
 * @param os Output stream to print stats to
 */
inline void comm::stats_print(const std::string &name, std::ostream &os) {
  std::stringstream sstr;
  sstr << "============== STATS =================\n"
       << "NAME                     = " << name << "\n"
       << "TIME                     = " << m_stats.get_elapsed_time() << "\n"
       << "GLOBAL_ASYNC_COUNT       = "
       << ::ygm::sum(m_stats.get_async_count(), *this) << "\n"
       << "GLOBAL_ISEND_COUNT       = "
       << ::ygm::sum(m_stats.get_isend_count(), *this) << "\n"
       << "GLOBAL_ISEND_BYTES       = "
       << ::ygm::sum(m_stats.get_isend_bytes(), *this) << "\n"
       << "MAX_WAITSOME_ISEND_IRECV = "
       << ::ygm::max(m_stats.get_waitsome_isend_irecv_time(), *this) << "\n"
       << "MAX_WAITSOME_IALLREDUCE  = "
       << ::ygm::max(m_stats.get_waitsome_iallreduce_time(), *this) << "\n"
       << "COUNT_IALLREDUCE         = " << m_stats.get_iallreduce_count()
       << "\n"
       << "======================================";

  if (rank0()) {
    os << sstr.str() << std::endl;
  }
}

/**
 * @brief Destructor for comm object
 *
 * @details Calls a barrier() to ensure all messages have been processed,
 * cancels all outstanding MPI receives and destroys MPI communicators set up
 * for use within the ygm::comm
 */
inline comm::~comm() {
  barrier();

  m_logger.log(log_level::info, "Destroying ygm::comm");

  YGM_ASSERT_RELEASE(MPI_Barrier(m_comm_async) == MPI_SUCCESS);

  YGM_ASSERT_RELEASE(m_send_queue.empty());
  YGM_ASSERT_RELEASE(m_send_local_dest_queue.empty());
  YGM_ASSERT_RELEASE(m_send_local_buffer_bytes == 0);
  YGM_ASSERT_RELEASE(m_send_remote_dest_queue.empty());
  YGM_ASSERT_RELEASE(m_send_remote_buffer_bytes == 0);
  YGM_ASSERT_RELEASE(m_pending_isend_bytes == 0);

  for (size_t i = 0; i < m_recv_queue.size(); ++i) {
    YGM_ASSERT_RELEASE(MPI_Cancel(&(m_recv_queue[i].request)) == MPI_SUCCESS);
  }
  YGM_ASSERT_RELEASE(MPI_Barrier(m_comm_async) == MPI_SUCCESS);
  YGM_ASSERT_RELEASE(MPI_Comm_free(&m_comm_async) == MPI_SUCCESS);
  YGM_ASSERT_RELEASE(MPI_Comm_free(&m_comm_barrier) == MPI_SUCCESS);
  YGM_ASSERT_RELEASE(MPI_Comm_free(&m_comm_other) == MPI_SUCCESS);

  pimpl_if.reset();
}

/**
 * @brief Asynchronous message initiation
 *
 * @tparam AsyncFunction Type of function object
 * @tparam SendArgs... Variadic type of arguments to send along with function.
 * All types must be serializable.
 * @param dest Rank to execute function on
 * @param fn Function object to execute at remote destination
 * @param args... Variadic arguments to send with message and pass to function
 * during execution
 * @details Serializes function object and queues for sending. Message will be
 * sent and executed at some future time that YGM deems appropriate.
 */
template <typename AsyncFunction, typename... SendArgs>
inline void comm::async(int dest, AsyncFunction &&fn, const SendArgs &...args) {
  YGM_CHECK_ASYNC_LAMBDA_COMPLIANCE(AsyncFunction, "ygm::comm::async()");

  YGM_ASSERT_RELEASE(dest < m_layout.size());
  m_stats.async(dest);

  check_if_production_halt_required();
  m_send_count++;

  //
  //
  int next_dest = dest;
  if (config.routing != detail::routing_type::NONE) {
    // next_dest = next_hop(dest);
    next_dest = m_router.next_hop(dest);
  }
  bool local = m_layout.is_local(next_dest);

  //
  // add data to the to dest buffer
  if (m_vec_send_buffers[next_dest].empty()) {
    if (local) {
      m_send_local_dest_queue.push_back(next_dest);
      m_vec_send_buffers[next_dest].reserve(config.local_buffer_size /
                                            m_layout.local_size());
    } else {
      m_send_remote_dest_queue.push_back(next_dest);
      m_vec_send_buffers[next_dest].reserve(config.remote_buffer_size /
                                            m_layout.node_size());
    }
  }

  // // Add header without message size
  size_t header_bytes = 0;
  if (config.routing != detail::routing_type::NONE) {
    header_bytes = pack_header(m_vec_send_buffers[next_dest], dest, 0);
    if (local) {
      m_send_local_buffer_bytes += header_bytes;
    } else {
      m_send_remote_buffer_bytes += header_bytes;
    }
  }

  uint32_t bytes = pack_lambda(m_vec_send_buffers[next_dest],
                               std::forward<AsyncFunction>(fn),
                               std::forward<const SendArgs>(args)...);
  if (local) {
    m_send_local_buffer_bytes += bytes;
  } else {
    m_send_remote_buffer_bytes += bytes;
  }

  // // Add message size to header
  if (config.routing != detail::routing_type::NONE) {
    auto iter = m_vec_send_buffers[next_dest].end();
    iter -= (header_bytes + bytes);
    std::memcpy(&*iter, &bytes, sizeof(header_t::message_size));
  }

  if (m_trace_ygm) {
    m_tracer.trace_ygm_async(m_tracer.get_next_message_id(), dest, bytes);
  }

  // Check if send buffer capacity has been exceeded
  flush_to_capacity();
}

/**
 * @brief Asynchronous message initiation for function that is sent to all ranks
 *
 * @tparam AsyncFunction Type of function object
 * @tparam SendArgs... Variadic type of arguments to send along with function.
 * All types must be serializable.
 * @param fn Function object to execute at remote destination
 * @param args... Variadic arguments to send with message and pass to function
 * during execution
 * @details Serializes function object and queues for sending to all ranks.
 * Message will be sent and executed at some future time that YGM deems
 * appropriate. Messages are sent along an implicitly defined broadcast tree
 * that takes advantage of knowledge of rank assignments to compute nodes.
 */
template <typename AsyncFunction, typename... SendArgs>
inline void comm::async_bcast(AsyncFunction &&fn, const SendArgs &...args) {
  YGM_CHECK_ASYNC_LAMBDA_COMPLIANCE(AsyncFunction, "ygm::comm::async_bcast()");

  check_if_production_halt_required();

  pack_lambda_broadcast(std::forward<AsyncFunction>(fn),
                        std::forward<const SendArgs>(args)...);

  //
  // Check if send buffer capacity has been exceeded
  flush_to_capacity();
}

template <typename AsyncFunction, typename... SendArgs>
inline void comm::async_mcast(const std::vector<int> &dests, AsyncFunction &&fn,
                              const SendArgs &...args) {
  YGM_CHECK_ASYNC_LAMBDA_COMPLIANCE(AsyncFunction, "ygm::comm::async_mcast()");

  for (auto dest : dests) {
    async(dest, std::forward<AsyncFunction>(fn),
          std::forward<const SendArgs>(args)...);
  }
}

/**
 * @brief Access to underlying layout object
 *
 * @return ygm::detail::layout object used by the ygm::comm
 */
inline const detail::layout &comm::layout() const { return m_layout; }

/**
 * @brief Access to underlying comm_router object
 *
 * @return ygm::detail::comm_router object used by the ygm::comm
 */
inline const detail::comm_router &comm::router() const { return m_router; }

/**
 * @brief Access to underlying comm_stats object
 *
 * @return ygm::detail::comm_stats object used by the ygm::comm
 */
inline const detail::comm_stats &comm::stats() const { return m_stats; }

/**
 * @brief Number of ranks in communicator
 *
 * @return Communicator size
 */
inline int comm::size() const { return m_layout.size(); }

/**
 * @brief Rank of the current process
 *
 * @return Rank within communicator
 * @details Ranks are unique IDs in the range [0, size-1] assigned to each
 * process in the communicator.
 */
inline int comm::rank() const { return m_layout.rank(); }

/**
 * @brief Access to copy of underlying MPI communicator
 *
 * @return Copy of MPI communicator distinct from one used for asynchronous
 * communication
 * @details Returned MPI_Comm is still managed by YGM and will be freed during
 * ygm::comm destructor.
 */
inline MPI_Comm comm::get_mpi_comm() const { return m_comm_other; }

/**
 * @brief Asynchronous communicator barrier
 *
 * @details An async_barrier can match with other async_barrier and barrier
 * calls. Any comm::barrier() calls matching with any async_barrier will execute
 * as expected but will not return until all ranks are in a non-async barrier.
 * The following code will complete successfully. If the calls to
 * async_barrier() were replaced with barrier(), the code would deadlock with
 * more than 1 rank. This call is useful when ranks may locally decide to run
 * more iterations of a loop than other ranks.
 * \code{cpp}
 *    for (int i=0; i<world.size(); ++i) {
 *      world.async_barrier();
 *    }
 *    world.barrier();
 * \endcode
 */
inline void comm::async_barrier() {
  log(log_level::debug, "Entering YGM async_barrier");
  bool ret = priv_barrier(false);
  YGM_ASSERT_RELEASE(ret == false);
  log(log_level::debug, "Exiting YGM async_barrier");
}

/**
 * @brief Full communicator barrier
 *
 * @details Collective operation that processes all messages (including any
 * recursively produced messages) on all ranks. All ranks must complete their
 * messages before any rank is able to return from the barrier() call.
 */
inline void comm::barrier() {
  if (m_trace_ygm || m_trace_mpi) {
    m_tracer.trace_barrier_begin(m_tracer.get_next_message_id(), m_send_count,
                                 m_recv_count, m_pending_isend_bytes,
                                 m_send_local_buffer_bytes,
                                 m_send_remote_buffer_bytes);
  }
  log(log_level::debug, "Entering YGM barrier");

  bool full_barrier = false;
  while (!full_barrier) {
    full_barrier = priv_barrier(true);
  }
  if (m_trace_ygm || m_trace_mpi) {
    m_tracer.trace_barrier_end(m_tracer.get_next_message_id(), m_send_count,
                               m_recv_count, m_pending_isend_bytes,
                               m_send_local_buffer_bytes,
                               m_send_remote_buffer_bytes);
  }
  log(log_level::debug, "Exiting YGM barrier");
}

/**
 * @brief barrier logic for both a full barrier and an async_barrier
 *
 * @param local_full is the local rank at a full global barrier
 * @return true the barrier was a full global barrier
 * @return false was not a full global barrier, at least one rank is at
 * async_barrier
 */
inline bool comm::priv_barrier(bool local_full) {
  flush_all_local_and_process_incoming();
  std::pair<uint64_t, uint64_t> previous_counts{1, 2};
  std::pair<uint64_t, uint64_t> current_counts{3, 4};
  while (!(current_counts.first == current_counts.second &&
           previous_counts == current_counts)) {
    previous_counts = current_counts;
    current_counts  = barrier_reduce_counts();
    if (current_counts.first != current_counts.second) {
      flush_all_local_and_process_incoming();
    }
  }

  YGM_ASSERT_RELEASE(m_pre_barrier_callbacks.empty());
  YGM_ASSERT_RELEASE(m_send_local_dest_queue.empty());
  YGM_ASSERT_RELEASE(m_send_remote_dest_queue.empty());

  bool to_return = false;
  YGM_ASSERT_MPI(MPI_Allreduce(&local_full, &to_return, 1, MPI_C_BOOL, MPI_LAND,
                               m_comm_barrier));
  return to_return;
}

/**
 * @brief Control Flow Barrier
 * Only blocks the control flow until all processes in the communicator have
 * called it. See:  MPI_Barrier()
 */
inline void comm::cf_barrier() const {
  log(log_level::debug, "Entering YGM cf_barrier");
  YGM_ASSERT_MPI(MPI_Barrier(m_comm_barrier));
  log(log_level::debug, "Exiting YGM cf_barrier");
}

template <typename T>
inline ygm_ptr<T> comm::make_ygm_ptr(T &t) {
  ygm_ptr<T> to_return(&t);
  to_return.check(*this);
  return to_return;
}

/**
 * @brief Registers a callback that will be executed prior to the barrier
 * completion
 *
 * @param fn callback function
 */
inline void comm::register_pre_barrier_callback(
    const std::function<void()> &fn) {
  m_pre_barrier_callbacks.push_back(fn);
}

/**
 * @warning Deprecated
 */
template <typename T>
inline T comm::all_reduce_sum(const T &t) const {
  T to_return;
  YGM_ASSERT_MPI(MPI_Allreduce(&t, &to_return, 1, detail::mpi_typeof(T()),
                               MPI_SUM, m_comm_other));
  return to_return;
}

/**
 * @warning Deprecated
 */
template <typename T>
inline T comm::all_reduce_min(const T &t) const {
  T to_return;
  YGM_ASSERT_MPI(MPI_Allreduce(&t, &to_return, 1, detail::mpi_typeof(T()),
                               MPI_MIN, m_comm_other));
  return to_return;
}

/**
 * @warning Deprecated
 */
template <typename T>
inline T comm::all_reduce_max(const T &t) const {
  T to_return;
  YGM_ASSERT_MPI(MPI_Allreduce(&t, &to_return, 1, detail::mpi_typeof(T()),
                               MPI_MAX, m_comm_other));
  return to_return;
}

/**
 * @warning Deprecated
 */
template <typename T, typename MergeFunction>
inline T comm::all_reduce(const T &in, MergeFunction merge) const {
  int first_child  = 2 * rank() + 1;
  int second_child = 2 * (rank() + 1);
  int parent       = (rank() - 1) / 2;

  // Step 1: Receive from children, merge into tmp
  T tmp = in;
  if (first_child < size()) {
    T fc = mpi_recv<T>(first_child, 0, m_comm_other);
    tmp  = merge(tmp, fc);
  }
  if (second_child < size()) {
    T sc = mpi_recv<T>(second_child, 0, m_comm_other);
    tmp  = merge(tmp, sc);
  }

  // Step 2: Send merged to parent
  if (rank() != 0) {
    mpi_send(tmp, parent, 0, m_comm_other);
  }

  // Step 3:  Rank 0 bcasts
  T to_return = mpi_bcast(tmp, 0, m_comm_other);
  return to_return;
}

/**
 * @brief Send an MPI message
 *
 * @tparam T datatype being sent (must be serializable with cereal)
 * @param data Message contents to send
 * @param dest Rank to send data to
 * @param tag MPI tag to assign to message
 * @param comm MPI communicator to send message over
 */
template <typename T>
inline void comm::mpi_send(const T &data, int dest, int tag,
                           MPI_Comm comm) const {
  ygm::detail::byte_vector packed;
  cereal::YGMOutputArchive oarchive(packed);
  oarchive(data);
  size_t packed_size = packed.size();
  YGM_ASSERT_RELEASE(packed_size < 1024 * 1024 * 1024);
  YGM_ASSERT_MPI(MPI_Send(&packed_size, 1, detail::mpi_typeof(packed_size),
                          dest, tag, comm));
  YGM_ASSERT_MPI(
      MPI_Send(packed.data(), packed_size, MPI_BYTE, dest, tag, comm));
}

/**
 * @brief Receive an MPI message
 *
 * @tparam T datatype being received (must be serializable with cereal)
 * @param source Rank sending message
 * @param tag MPI tag to assign to message
 * @param comm MPI communicator message is being sent over
 * @return Received message
 */
template <typename T>
inline T comm::mpi_recv(int source, int tag, MPI_Comm comm) const {
  std::vector<std::byte> packed;
  size_t                 packed_size{0};
  YGM_ASSERT_MPI(MPI_Recv(&packed_size, 1, detail::mpi_typeof(packed_size),
                          source, tag, comm, MPI_STATUS_IGNORE));
  packed.resize(packed_size);
  YGM_ASSERT_MPI(MPI_Recv(packed.data(), packed_size, MPI_BYTE, source, tag,
                          comm, MPI_STATUS_IGNORE));

  T                       to_return;
  cereal::YGMInputArchive iarchive(packed.data(), packed.size());
  iarchive(to_return);
  return to_return;
}

/**
 * @brief Broadcast an MPI message
 *
 * @tparam Datatype to broadcast (must be serializable)
 * @param to_bcast Data being broadcast
 * @param root Rank message is being broadcast from
 * @param comm MPI communicator message is being broadcast over
 * @return Data received from root
 */
template <typename T>
inline T comm::mpi_bcast(const T &to_bcast, int root, MPI_Comm comm) const {
  ygm::detail::byte_vector packed;
  cereal::YGMOutputArchive oarchive(packed);
  if (rank() == root) {
    oarchive(to_bcast);
  }
  size_t packed_size = packed.size();
  YGM_ASSERT_RELEASE(packed_size < 1024 * 1024 * 1024);
  YGM_ASSERT_MPI(
      MPI_Bcast(&packed_size, 1, detail::mpi_typeof(packed_size), root, comm));
  if (rank() != root) {
    packed.resize(packed_size);
  }
  YGM_ASSERT_MPI(MPI_Bcast(packed.data(), packed_size, MPI_BYTE, root, comm));

  cereal::YGMInputArchive iarchive(packed.data(), packed.size());
  T                       to_return;
  iarchive(to_return);
  return to_return;
}

/**
 * @brief Provides a std::cout ostream that is only writeable from rank 0
 *
 * @return std::cout that only writes from rank 0
 *
 * \code{cpp}
 *    world.cout0() << "This output is coming from rank 0" << std::endl;
 * \endcode
 *
 */
inline std::ostream &comm::cout0() const {
  if (rank() == 0) {
    return std::cout;
  }
  return detail::dummy_ostream();
}

/**
 * @brief Provides a std::cerr ostream that is only writeable from rank 0
 *
 * @return std::cerr that only writes from rank 0
 */
inline std::ostream &comm::cerr0() const {
  if (rank() == 0) {
    return std::cerr;
  }
  return detail::dummy_ostream();
}

/**
 * @brief Provides std::cout access with each line labeled by the rank producing
 * the output
 *
 * @return std::cout for use by any rank
 */
inline std::ostream &comm::cout() const {
  std::cout << rank() << ": ";
  return std::cout;
}

/**
 * @brief Provides std::cerr access with each line labeled by the rank producing
 * the output
 *
 * @return std::cerr for use by any rank
 */
inline std::ostream &comm::cerr() const {
  std::cerr << rank() << ": ";
  return std::cerr;
}

/**
 * @brief python print-like function that writes to std::cout from only rank 0
 *
 * @tparam Args... Variadic argument types to print
 * @param args... Variadic arguments for printing
 *
 * \code{cpp}
 *    world.cout0("Printing from rank 0 only")
 * \endcode
 */
template <typename... Args>
inline void comm::cout0(Args &&...args) const {
  if (rank0()) {
    std::cout << detail::outstr0(args...) << std::endl;
  }
}

/**
 * @brief python print-like function that writes to std::cerr from only rank 0
 *
 * @tparam Args... Variadic argument types to print
 * @param args... Variadic arguments for printing
 */
template <typename... Args>
inline void comm::cerr0(Args &&...args) const {
  if (rank0()) {
    std::cerr << detail::outstr0(args...) << std::endl;
  }
}

/**
 * @brief python print-like function that writes to std::cout from any rank
 *
 * @tparam Args... Variadic argument types to print
 * @param args... Variadic arguments for printing
 *
 * \code{cpp}
 *    world.cout("Printing from every rank")
 * \endcode
 */
template <typename... Args>
inline void comm::cout(Args &&...args) const {
  std::cout << detail::outstr(args...) << std::endl;
}

/**
 * @brief python print-like function that writes to std::cerr from any rank
 *
 * @tparam Args... Variadic argument types to print
 * @param args... Variadic arguments for printing
 *
 * \code{cpp}
 *    world.cerr("Printing from every rank")
 * \endcode
 */
template <typename... Args>
inline void comm::cerr(Args &&...args) const {
  std::cerr << detail::outstr(args...) << std::endl;
}

/**
 * @brief Serializes routing headers
 *
 * @param packed Serialized messages to append header for next message to
 * @param dest Destination rank to place in header
 * @param size Number of bytes in message associated with header
 * @return Size of header to message
 */
inline size_t comm::pack_header(ygm::detail::byte_vector &packed,
                                const int dest, size_t size) {
  size_t size_before = packed.size();

  header_t h;
  h.dest         = dest;
  h.message_size = size;

  packed.push_bytes(&h, sizeof(header_t));
  // cereal::YGMOutputArchive oarchive(packed);
  // oarchive(h);

  return packed.size() - size_before;
}

/**
 * @brief Collects number of messages sent and received globally
 *
 * @return Pair containing number of messages received and number of messages
 * sent globally
 * @details Counts returned are for every rank at their time of entry to
 * barrier_reduce_counts(). Ranks may enter this function at different times, so
 * reductions of counts is happening asynchronously. While waiting for
 * asynchronous reductions to complete, ranks will continue attempting to
 * receive and process incoming YGM messages.
 */
inline std::pair<uint64_t, uint64_t> comm::barrier_reduce_counts() {
  uint64_t local_counts[2]  = {m_recv_count, m_send_count};
  uint64_t global_counts[2] = {0, 0};

  YGM_ASSERT_RELEASE(m_pending_isend_bytes == 0);
  YGM_ASSERT_RELEASE(m_send_local_buffer_bytes == 0);
  YGM_ASSERT_RELEASE(m_send_remote_buffer_bytes == 0);

  MPI_Request req = MPI_REQUEST_NULL;
  YGM_ASSERT_MPI(MPI_Iallreduce(local_counts, global_counts, 2, MPI_UINT64_T,
                                MPI_SUM, m_comm_barrier, &req));
  m_stats.iallreduce();
  bool iallreduce_complete(false);
  while (!iallreduce_complete) {
    MPI_Request twin_req[2];
    twin_req[0] = req;
    twin_req[1] = m_recv_queue.front().request;

    int        outcount{0};
    int        twin_indices[2];
    MPI_Status twin_status[2];

    {
      auto timer = m_stats.waitsome_iallreduce();
      while (outcount == 0) {
        YGM_ASSERT_MPI(
            MPI_Testsome(2, twin_req, &outcount, twin_indices, twin_status));
      }
    }

    for (int i = 0; i < outcount; ++i) {
      if (twin_indices[i] == 0) {  // completed a Iallreduce
        iallreduce_complete = true;
        // std::cout << m_layout.rank() << ": iallreduce_complete: " <<
        // global_counts[0] << " " << global_counts[1] << std::endl;
      } else {
        mpi_irecv_request req_buffer = m_recv_queue.front();
        m_recv_queue.pop_front();
        int buffer_size{0};
        YGM_ASSERT_MPI(MPI_Get_count(&twin_status[i], MPI_BYTE, &buffer_size));
        m_stats.irecv(twin_status[i].MPI_SOURCE, buffer_size);

        handle_next_receive(req_buffer.buffer, buffer_size,
                            twin_status[i].MPI_SOURCE);
        flush_all_local_and_process_incoming();
      }
    }
  }
  return {global_counts[0], global_counts[1]};
}

/**
 * @brief Flushes send buffer to dest
 *
 * @param dest
 */
inline void comm::flush_send_buffer(int dest) {
  static size_t counter = 0;
  if (m_vec_send_buffers[dest].size() > 0) {
    check_completed_sends();
    mpi_isend_request request;

    if (m_trace_mpi) {
      request.start_id = m_tracer.get_next_message_id();
    } else {
      request.start_id = 0;
    }

    if (m_free_send_buffers.empty()) {
      request.buffer = std::make_shared<ygm::detail::byte_vector>();
    } else {
      request.buffer = m_free_send_buffers.back();
      m_free_send_buffers.pop_back();
    }
    request.buffer->swap(m_vec_send_buffers[dest]);
    if (config.freq_issend > 0 && counter++ % config.freq_issend == 0) {
      log(log_level::debug, "MPI_Issend " +
                                std::to_string(request.buffer->size()) +
                                " bytes to rank " + std::to_string(dest));
      YGM_ASSERT_MPI(MPI_Issend(request.buffer->data(), request.buffer->size(),
                                MPI_BYTE, dest, 0, m_comm_async,
                                &(request.request)));
    } else {
      log(log_level::debug, "MPI_Isend " +
                                std::to_string(request.buffer->size()) +
                                " bytes to rank " + std::to_string(dest));
      YGM_ASSERT_MPI(MPI_Isend(request.buffer->data(), request.buffer->size(),
                               MPI_BYTE, dest, 0, m_comm_async,
                               &(request.request)));
    }
    m_stats.isend(dest, request.buffer->size());

    m_pending_isend_bytes += request.buffer->size();

    if (m_layout.is_local(dest)) {
      m_send_local_buffer_bytes -= request.buffer->size();
    } else {
      m_send_remote_buffer_bytes -= request.buffer->size();
    }

    if (m_trace_mpi) {
      m_tracer.trace_mpi_send(request.start_id, dest, request.buffer->size());
    }

    m_send_queue.push_back(request);
    if (!m_in_process_receive_queue) {
      process_receive_queue();
    }
  }
}

/**
 * @brief Flushes first queued buffer of messages
 *
 * @param dest_queue Queue of destinations to send buffered messages to
 */
inline void comm::flush_next_send(std::deque<int> &dest_queue) {
  if (!dest_queue.empty()) {
    int dest = dest_queue.front();
    dest_queue.pop_front();
    flush_send_buffer(dest);
  }
}

/**
 * @brief Handle a completed send by putting the buffer on the free list or
 * allowing it to be freed
 *
 * @param req_buffer mpi_isend_request object associated to completed send
 */
inline void comm::handle_completed_send(mpi_isend_request &req_buffer) {
  m_pending_isend_bytes -= req_buffer.buffer->size();
  log(log_level::debug, "Completed send of " +
                            std::to_string(req_buffer.buffer->size()) +
                            " bytes");
  if (m_free_send_buffers.size() < config.send_buffer_free_list_len) {
    req_buffer.buffer->clear();
    m_free_send_buffers.push_back(req_buffer.buffer);
  }
}

/**
 * @brief Test completed sends
 */
inline void comm::check_completed_sends() {
  if (!m_send_queue.empty()) {
    int flag(1);
    while (flag && not m_send_queue.empty()) {
      YGM_ASSERT_MPI(
          MPI_Test(&(m_send_queue.front().request), &flag, MPI_STATUS_IGNORE));
      m_stats.isend_test();
      if (flag) {
        if (m_trace_mpi) {
          m_tracer.trace_mpi_send_complete(m_tracer.get_next_message_id(),
                                           m_send_queue.front().start_id,
                                           m_send_queue.front().buffer->size());
        }
        handle_completed_send(m_send_queue.front());
        m_send_queue.pop_front();
      }
    }
  }
}

/**
 * @brief Temporarily prevent creation of new messages
 *
 * @details While avoiding the creation of new messages, received messages will
 * be processes in process_receive_queue. While already processing incoming
 * messages, new message creation is not prevented as we cannot safely re-enter
 * process_receive_queue(). When interrupts are disabled within YGM, this
 * function also does nothing because an application has indicated it is not
 * safe to let YGM start processing new messages.
 *
 */
inline void comm::check_if_production_halt_required() {
  while (m_enable_interrupts && !m_in_process_receive_queue &&
         m_pending_isend_bytes >
             (config.local_buffer_size + config.remote_buffer_size)) {
    process_receive_queue();
  }
}

/**
 * @brief Checks for incoming unless called from receive queue and flushes
 * one buffer.
 */
inline void comm::local_progress() {
  if (not m_in_process_receive_queue) {
    process_receive_queue();
  }
  if (not m_send_local_dest_queue.empty()) {
    flush_next_send(m_send_local_dest_queue);
  }
  if (not m_send_remote_dest_queue.empty()) {
    flush_next_send(m_send_remote_dest_queue);
  }
}

/**
 * @brief Waits until provided condition function returns true.
 *
 * @tparam Function functor type
 * @param fn Wait condition function, must match []() -> bool
 *
 * This is useful when applications can determine locally that their part of a
 * computation is complete (or nearly complete). This can be used to completely
 * avoid barrier() calls or reduce the number of reductions needed within a
 * barrier() to reach quiescence.
 * \code{cpp}
 *    static int messages_received;
 *    messages_received = 0;
 *    for (int i=0; i<world.rank(); ++i) {
 *        world.async(i, [](){++messages_received;});
 *    }
 *
 *    world.local_wait_until([&world](){return messages_received ==
 * world.rank()});
 * \endcode
 */
template <typename Function>
inline void comm::local_wait_until(Function fn) {
  while (not fn()) {
    local_progress();
  }
}

/**
 * @brief Flushes all local state and buffers.
 * Notifies any registered barrier watchers.
 */
inline void comm::flush_all_local_and_process_incoming() {
  // Keep flushing until all local work is complete
  bool did_something = true;
  while (did_something) {
    did_something = process_receive_queue();
    //
    //  Notify registered barrier watchers
    while (!m_pre_barrier_callbacks.empty()) {
      did_something            = true;
      std::function<void()> fn = m_pre_barrier_callbacks.front();
      m_pre_barrier_callbacks.pop_front();
      fn();
    }

    //
    //  Flush each send buffer
    while (!m_send_local_dest_queue.empty()) {
      did_something = true;
      flush_next_send(m_send_local_dest_queue);
      process_receive_queue();
    }
    while (!m_send_remote_dest_queue.empty()) {
      did_something = true;
      flush_next_send(m_send_remote_dest_queue);
      process_receive_queue();
    }

    //
    // Wait on isends
    while (!m_send_queue.empty()) {
      did_something |= process_receive_queue();
    }
  }
}

/**
 * @brief Flush send buffers until queued sends are smaller than buffer
 * capacity
 */
inline void comm::flush_to_capacity() {
  while (m_send_local_buffer_bytes > config.local_buffer_size) {
    YGM_ASSERT_DEBUG(!m_send_local_dest_queue.empty());
    flush_next_send(m_send_local_dest_queue);
  }
  while (m_send_remote_buffer_bytes > config.remote_buffer_size) {
    YGM_ASSERT_DEBUG(!m_send_remote_dest_queue.empty());
    flush_next_send(m_send_remote_dest_queue);
  }
}

/**
 * @brief Posts a new receive waiting for incoming messages
 *
 * @param recv_buffer A shared pointer to the buffer to store the incoming
 * message
 * @details The buffer is cleared before posting the receive.
 */
inline void comm::post_new_irecv(
    std::shared_ptr<ygm::detail::byte_vector> &recv_buffer) {
  recv_buffer->clear();
  mpi_irecv_request recv_req;
  recv_req.buffer = recv_buffer;

  //::madvise(recv_req.buffer.get(), config.irecv_size, MADV_DONTNEED);
  YGM_ASSERT_MPI(MPI_Irecv(recv_req.buffer.get()->data(), config.irecv_size,
                           MPI_BYTE, MPI_ANY_SOURCE, MPI_ANY_TAG, m_comm_async,
                           &(recv_req.request)));
  m_recv_queue.push_back(recv_req);
}

/**
 * @brief Serializes the user's function and its arguments to execute remotely
 * within a buffer on a route to that destination
 *
 * @tparam Lambda Type of user's function
 * @tparam PackArgs... Variadic types of arguments to user's function
 * @param packed Serialized messages to append packed function to
 * @param l User's function
 * @param args... Arguments to pass to user's function
 * @return Size of serialized function and arguments
 *
 * @details This function wraps the user's lambda in another lambda that
 * contains the logic for deserializing arguments for the user's lambda and then
 * executing it. The final serialization and wrapping of this lambda occurs in
 * comm::pack_lambda_generic.
 */
template <typename Lambda, typename... PackArgs>
inline size_t comm::pack_lambda(ygm::detail::byte_vector &packed, Lambda &&l,
                                const PackArgs &...args) {
  auto dispatch_lambda = [](comm *c, cereal::YGMInputArchive *bia,
                            std::remove_reference_t<Lambda> &&l) {
    std::tuple<PackArgs...> ta;
    if constexpr (!std::is_empty<std::tuple<PackArgs...>>::value) {
      (*bia)(ta);
    }

    auto t1 = std::make_tuple((comm *)c);

    // \pp was: std::apply(*pl, std::tuple_cat(t1, ta));
    ygm::meta::apply_optional(l, std::move(t1), std::move(ta));
  };

  return pack_lambda_generic(packed, std::forward<Lambda>(l), dispatch_lambda,
                             std::forward<const PackArgs>(args)...);
}

/**
 * @brief Serializes the user's function and its arguments for an asynchronous
 * broadcast operation.
 *
 * @tparam Lambda Type of user's function
 * @tparam PackArgs... Variadic types of arguments to user's function
 * @param l User's function
 * @param args... Arguments to pass to user's function
 * @return Size of serialized function and arguments
 *
 * @details This function wraps the user's lambda in another lambda that
 * contains the logic for deserializing arguments for the user's lambda and then
 * executing it. In the case of async_bcast, that wrapper also copies the
 * message and arguments to the next destinations along an implicit braodcast
 * tree that will reach all destination ranks. The final serialization and
 * wrapping of this lambda occurs in comm::pack_lambda_generic.
 */
template <typename Lambda, typename... PackArgs>
inline void comm::pack_lambda_broadcast(Lambda &&l, const PackArgs &...args) {
  auto forward_remote_and_dispatch_lambda = [](comm                    *c,
                                               cereal::YGMInputArchive *bia,
                                               std::remove_reference_t<Lambda>
                                                   l) {
    std::tuple<PackArgs...> ta;
    if constexpr (!std::is_empty<std::tuple<PackArgs...>>::value) {
      (*bia)(ta);
    }

    auto forward_local_and_dispatch_lambda =
        [](comm *c, cereal::YGMInputArchive *bia,
           std::remove_reference_t<Lambda> l) {
          std::tuple<PackArgs...> ta;
          if constexpr (!std::is_empty<std::tuple<PackArgs...>>::value) {
            (*bia)(ta);
          }

          auto local_dispatch_lambda = [](comm *c, cereal::YGMInputArchive *bia,
                                          std::remove_reference_t<Lambda> l) {
            std::tuple<PackArgs...> ta;
            if constexpr (!std::is_empty<std::tuple<PackArgs...>>::value) {
              (*bia)(ta);
            }

            auto t1 = std::make_tuple((comm *)c);

            // \pp was: std::apply(*pl, std::tuple_cat(t1, ta));
            ygm::meta::apply_optional(l, std::move(t1), std::move(ta));
          };

          // Pack lambda telling terminal ranks to execute user lambda.
          // TODO: Why does this work? Passing ta (tuple of args) to a
          // function expecting a parameter pack shouldn't work...
          ygm::detail::byte_vector packed_msg;
          c->pack_lambda_generic(packed_msg, l, local_dispatch_lambda, ta);

          for (auto dest : c->layout().local_ranks()) {
            if (dest != c->layout().rank()) {
              c->queue_message_bytes(packed_msg, dest);
            }
          }

          auto t1 = std::make_tuple((comm *)c);

          // \pp was: std::apply(*pl, std::tuple_cat(t1, ta));
          ygm::meta::apply_optional(l, std::move(t1), std::move(ta));
        };

    ygm::detail::byte_vector packed_msg;
    c->pack_lambda_generic(packed_msg, l, forward_local_and_dispatch_lambda,
                           ta);

    int num_layers = c->layout().node_size() / c->layout().local_size() +
                     (c->layout().node_size() % c->layout().local_size() > 0);
    int num_ranks_per_layer =
        c->layout().local_size() * c->layout().local_size();
    int node_partner_offset = (c->layout().local_id() - c->layout().node_id()) %
                              c->layout().local_size();

    // % operator is remainder, not actually mod. Need to fix result if
    // result was negative
    if (node_partner_offset < 0) {
      node_partner_offset += c->layout().local_size();
    }

    // Only forward remotely if initial remote node exists
    if (node_partner_offset < c->layout().node_size()) {
      int curr_partner = c->layout().strided_ranks()[node_partner_offset];
      for (int l = 0; l < num_layers; l++) {
        if (curr_partner >= c->layout().size()) {
          break;
        }
        if (!c->layout().is_local(curr_partner)) {
          c->queue_message_bytes(packed_msg, curr_partner);
        }

        curr_partner += num_ranks_per_layer;
      }
    }

    auto t1 = std::make_tuple((comm *)c);

    // \pp was: std::apply(*pl, std::tuple_cat(t1, ta));
    ygm::meta::apply_optional(l, std::move(t1), std::move(ta));
  };

  ygm::detail::byte_vector packed_msg;
  pack_lambda_generic(packed_msg, std::forward<Lambda>(l),
                      forward_remote_and_dispatch_lambda,
                      std::forward<const PackArgs>(args)...);

  // Initial send to all local ranks
  for (auto dest : layout().local_ranks()) {
    queue_message_bytes(packed_msg, dest);
  }
}

/**
 * @brief Final wrapping of user-provided lambdas inside of another lambda that
 * contains the logic for handling the message at the next rank along its route
 * to the destination.
 *
 * @tparam Lambda Type of user-provided lambda
 * @tparam RemoteLogicLambda Type of lambda providing logic for handling message
 * at remote rank
 * @tparam PackArgs... Variadic types for user-provide function arguments
 * @param packed Buffer of messages to serialize function and arguments into
 * @param l User-provided lambda to execute at destination rank
 * @param rll Lambda created by YGM containing logic for possibly executing or
 * forwarding message at next remote rank
 * @param args... Variadic arguments to user's lambda
 * @return Number of bytes added by serializing function and arguments
 */
template <typename Lambda, typename RemoteLogicLambda, typename... PackArgs>
inline size_t comm::pack_lambda_generic(ygm::detail::byte_vector &packed,
                                        Lambda &&l, RemoteLogicLambda rll,
                                        const PackArgs &...args) {
  size_t                        size_before = packed.size();
  const std::tuple<PackArgs...> tuple_args(
      std::forward<const PackArgs>(args)...);

  // Lambda that initially executes on remote rank. Deserializes the user's
  // lambda and the lambda containing the logic for actions to perform at remote
  // rank and then execute the RemoteLogicLambda with the user's lambda as an
  // argument.
  auto remote_dispatch_lambda = [](comm *c, cereal::YGMInputArchive *bia) {
    std::remove_reference_t<Lambda> *pl  = nullptr;
    RemoteLogicLambda               *rll = nullptr;

    // Deserialize captured values from RemoteLogicLambda and Lambda
    size_t rll_storage[sizeof(RemoteLogicLambda) / sizeof(size_t) +
                       (sizeof(RemoteLogicLambda) % sizeof(size_t) > 0)];
    if constexpr (!std::is_empty<RemoteLogicLambda>::value) {
      bia->loadBinary(rll_storage, sizeof(RemoteLogicLambda));
      rll = (RemoteLogicLambda *)rll_storage;
    }

    size_t l_storage[sizeof(std::remove_reference_t<Lambda>) / sizeof(size_t) +
                     (sizeof(std::remove_reference_t<Lambda>) % sizeof(size_t) >
                      0)];
    if constexpr (!std::is_empty<std::remove_reference_t<Lambda>>::value) {
      bia->loadBinary(l_storage, sizeof(std::remove_reference_t<Lambda>));
      pl = (std::remove_reference_t<Lambda> *)l_storage;
    }

    (*rll)(c, bia, std::move(*pl));
  };

  uint16_t lid = m_lambda_map.register_lambda(remote_dispatch_lambda);

  {
    packed.push_bytes(&lid, sizeof(lid));
  }

  if constexpr (!std::is_empty<RemoteLogicLambda>::value) {
    packed.push_bytes(&rll, sizeof(RemoteLogicLambda));
  }

  if constexpr (!std::is_empty<std::remove_reference_t<Lambda>>::value) {
    packed.push_bytes(&l, sizeof(std::remove_reference_t<Lambda>));
  }

  if constexpr (!std::is_empty<std::tuple<PackArgs...>>::value) {
    // Only create cereal archive is tuple needs serialization
    cereal::YGMOutputArchive oarchive(packed);  // Create an output archive
    oarchive(tuple_args);
  }
  return packed.size() - size_before;
}

/**
 * @brief Adds packed message directly to send buffer for specific
 * destination. Does not modify packed message to add headers for routing.
 *
 * @param packed Buffer containing a single serialized message to queue to its
 * destination
 * @param dest Destination for packed message
 */
inline void comm::queue_message_bytes(const ygm::detail::byte_vector &packed,
                                      const int                       dest) {
  m_send_count++;
  bool local = m_layout.is_local(dest);
  //
  // add data to the dest buffer
  if (m_vec_send_buffers[dest].empty()) {
    if (local) {
      m_send_local_dest_queue.push_back(dest);
      m_vec_send_buffers[dest].reserve(config.local_buffer_size /
                                       m_layout.local_size());
    } else {
      m_send_remote_dest_queue.push_back(dest);
      m_vec_send_buffers[dest].reserve(config.remote_buffer_size /
                                       m_layout.node_size());
    }
  }

  ygm::detail::byte_vector &send_buff = m_vec_send_buffers[dest];

  // Add dummy header with dest of -1 and size of 0.
  // This is to avoid peeling off and replacing the dest as messages are
  // forwarded in a bcast
  if (config.routing != detail::routing_type::NONE) {
    size_t header_bytes = pack_header(send_buff, -1, 0);
    if (local) {
      m_send_local_buffer_bytes += header_bytes;
    } else {
      m_send_remote_buffer_bytes += header_bytes;
    }
  }

  send_buff.push_bytes(packed.data(), packed.size());
  if (local) {
    m_send_local_buffer_bytes += packed.size();
  } else {
    m_send_remote_buffer_bytes += packed.size();
  }
}

/**
 * @brief Deserializes and processes messages in a buffer of received messages
 *
 * @param buffer Shared pointer to buffer of received messages
 * @param buffer_size Size of received message buffer
 * @param from_rank Rank that sent buffer
 */
inline void comm::handle_next_receive(
    std::shared_ptr<ygm::detail::byte_vector> &buffer, const size_t buffer_size,
    const uint32_t from_rank) {
  log(log_level::debug, "Received " + std::to_string(buffer_size) +
                            " bytes from rank " + std::to_string(from_rank));

  if (m_trace_mpi) {
    m_tracer.trace_mpi_recv(m_tracer.get_next_message_id(), from_rank,
                            buffer_size);
  }

  cereal::YGMInputArchive iarchive(buffer.get()->data(), buffer_size);
  // Loop over messages in buffer and handle each
  while (!iarchive.empty()) {
    if (config.routing != detail::routing_type::NONE) {
      header_t h;
      iarchive.loadBinary(&h, sizeof(header_t));

      if (h.dest == m_layout.rank() || (h.dest == -1 && h.message_size == 0)) {
        uint16_t lid;
        iarchive.loadBinary(&lid, sizeof(lid));
        m_lambda_map.execute(lid, this, &iarchive);
        m_recv_count++;
        m_stats.rpc_execute();
      } else {
        int  next_dest = m_router.next_hop(h.dest);
        bool local     = m_layout.is_local(next_dest);

        if (m_vec_send_buffers[next_dest].empty()) {
          if (local) {
            m_send_local_dest_queue.push_back(next_dest);
          } else {
            m_send_remote_dest_queue.push_back(next_dest);
          }
        }

        size_t header_bytes =
            pack_header(m_vec_send_buffers[next_dest], h.dest, h.message_size);
        if (local) {
          m_send_local_buffer_bytes += header_bytes;
        } else {
          m_send_remote_buffer_bytes += header_bytes;
        }

        size_t precopy_size = m_vec_send_buffers[next_dest].size();
        m_vec_send_buffers[next_dest].resize(precopy_size + h.message_size);
        iarchive.loadBinary(&m_vec_send_buffers[next_dest][precopy_size],
                            h.message_size);
        if (local) {
          m_send_local_buffer_bytes += h.message_size;
        } else {
          m_send_remote_buffer_bytes += h.message_size;
        }

        flush_to_capacity();
      }
    } else {
      uint16_t lid;
      iarchive.loadBinary(&lid, sizeof(lid));
      m_lambda_map.execute(lid, this, &iarchive);
      m_recv_count++;
      m_stats.rpc_execute();
    }
  }
  post_new_irecv(buffer);
  flush_to_capacity();
}

/**
 * @brief Process receive queue of messages received.
 *
 * @return True if receive queue was non-empty, else false
 */
inline bool comm::process_receive_queue() {
  YGM_ASSERT_RELEASE(!m_in_process_receive_queue);
  m_in_process_receive_queue = true;
  bool received_to_return    = false;

  if (!m_enable_interrupts) {
    m_in_process_receive_queue = false;
    return received_to_return;
  }
  //
  // if we have a pending iRecv, then we can issue a Testsome
  if (m_send_queue.size() > config.num_isends_wait) {
    MPI_Request twin_req[2];
    twin_req[0] = m_send_queue.front().request;
    twin_req[1] = m_recv_queue.front().request;

    int        outcount{0};
    int        twin_indices[2];
    MPI_Status twin_status[2];
    {
      auto timer = m_stats.waitsome_isend_irecv();
      while (outcount == 0) {
        YGM_ASSERT_MPI(
            MPI_Testsome(2, twin_req, &outcount, twin_indices, twin_status));
      }
    }
    for (int i = 0; i < outcount; ++i) {
      if (twin_indices[i] == 0) {  // completed a iSend
        handle_completed_send(m_send_queue.front());
        m_send_queue.pop_front();
      } else {  // completed an iRecv -- COPIED FROM BELOW
        received_to_return           = true;
        mpi_irecv_request req_buffer = m_recv_queue.front();
        m_recv_queue.pop_front();
        int buffer_size{0};
        YGM_ASSERT_MPI(MPI_Get_count(&twin_status[i], MPI_BYTE, &buffer_size));
        m_stats.irecv(twin_status[i].MPI_SOURCE, buffer_size);

        handle_next_receive(req_buffer.buffer, buffer_size,
                            twin_status[i].MPI_SOURCE);
      }
    }
  } else {
    check_completed_sends();
  }

  received_to_return |= local_process_incoming();

  m_in_process_receive_queue = false;
  return received_to_return;
}

/**
 * @brief Check for incoming messages and continue processing until no messages
 * are found
 *
 * @return True if any messages were received, otherwise false.
 */
inline bool comm::local_process_incoming() {
  bool received_to_return = false;

  while (true) {
    int        flag(0);
    MPI_Status status;
    YGM_ASSERT_MPI(MPI_Test(&(m_recv_queue.front().request), &flag, &status));
    m_stats.irecv_test();
    if (flag) {
      received_to_return           = true;
      mpi_irecv_request req_buffer = m_recv_queue.front();
      m_recv_queue.pop_front();
      int buffer_size{0};
      YGM_ASSERT_MPI(MPI_Get_count(&status, MPI_BYTE, &buffer_size));
      m_stats.irecv(status.MPI_SOURCE, buffer_size);

      handle_next_receive(req_buffer.buffer, buffer_size, status.MPI_SOURCE);
    } else {
      break;  // not ready yet
    }
  }
  return received_to_return;
}

/**
 * @brief Turn on tracing of YGM functions
 *
 * This is more granular than MPI tracing. YGM tracing occurs at the level of
 * individual async calls and is indicative of the calls requested by an
 * application. MPI tracing occurs at the level of buffers sent through YGM and
 * is indicative of the communication YGM actually performed to meet the
 * requests of the application's async calls.
 */
inline void comm::enable_ygm_tracing() {
  // Setup tracing if not already enabled
  if (!m_trace_ygm && !m_trace_mpi) {
    m_tracer.create_directory();
    cf_barrier();
    m_tracer.open_file();
  }
  m_trace_ygm = true;
}

/**
 * @brief Turn on tracing of MPI calls within YGM
 */
inline void comm::enable_mpi_tracing() {
  // Setup tracing if not already enabled
  if (!m_trace_ygm && !m_trace_mpi) {
    m_tracer.create_directory();
    cf_barrier();
    m_tracer.open_file();
  }
  m_trace_mpi = true;
}

/**
 * @brief Turn off tracing of YGM functions
 */
inline void comm::disable_ygm_tracing() {
  m_trace_ygm = false;
  cf_barrier();
  // if (!m_trace_ygm && !m_trace_mpi) {
  //   m_tracer.close_file();
  //   cf_barrier();
  // }
}

/**
 * @brief Turn off tracing of MPI calls
 */
inline void comm::disable_mpi_tracing() {
  m_trace_mpi = false;
  cf_barrier();

  // if (!m_trace_ygm && !m_trace_mpi) {
  //   m_tracer.close_file();
  //   cf_barrier();
  // }
}

/**
 * @brief Check status of YGM tracing
 *
 * @return True if currently tracing YGM functions, otherwise false
 */
inline bool comm::is_ygm_tracing_enabled() const { return m_trace_ygm; }

/**
 * @brief Check status of MPI tracing
 *
 * @return True if currently tracing MPI calls, otherwise false
 */
inline bool comm::is_mpi_tracing_enabled() const { return m_trace_mpi; }

/**
 * @brief Set the location of the YGM log files. One file will be created at
 * this location for every rank.
 *
 * @tparam StringType Type of provided path as string. Must be convertible to
 * std::filesystem::path.
 * @param s Path to log location as a string
 */
template <typename StringType>
inline void comm::set_log_location(const StringType &s) {
  set_log_location(std::filesystem::path(s));
}

/**
 * @brief Set the location of the YGM log files. One file will be created at
 * this location for every rank.
 *
 * @param p Path to log location as an std::filesystem::path
 */
inline void comm::set_log_location(std::filesystem::path p) {
  // p will be treated as a desired directory location to store all logs. The
  // full name of the individual loggers on each rank will be determined by the
  // ygm::detail::logger objects.
  if (std::filesystem::exists(p) && not std::filesystem::is_directory(p)) {
    cout0("Cannot set log location: ", p, " exists and is not a directory");
  }
  std::filesystem::create_directories(p);

  p /= ("ygm_logs" + std::to_string(rank()));
  m_logger.set_path(p);
}

};  // namespace ygm
