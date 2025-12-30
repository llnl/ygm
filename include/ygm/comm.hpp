// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <deque>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include <ygm/detail/byte_vector.hpp>
#include <ygm/detail/comm_environment.hpp>
#include <ygm/detail/comm_router.hpp>
#include <ygm/detail/comm_stats.hpp>
#include <ygm/detail/lambda_map.hpp>
#include <ygm/detail/layout.hpp>
#include <ygm/detail/logger.hpp>
#include <ygm/detail/meta/functional.hpp>
#include <ygm/detail/mpi.hpp>
#include <ygm/detail/tracer.hpp>
#include <ygm/detail/ygm_cereal_archive.hpp>
#include <ygm/utility/world.hpp>

namespace ygm {

template <typename T>
class ygm_ptr;

namespace detail {
class interrupt_mask;
class comm_stats;
class layout;
class comm_router;
class tracer;
}  // namespace detail

class comm {
 private:
  struct mpi_irecv_request {
    std::shared_ptr<ygm::detail::byte_vector> buffer;
    MPI_Request                               request;
  };

  struct mpi_isend_request {
    std::shared_ptr<ygm::detail::byte_vector> buffer;
    MPI_Request                               request;
    int32_t                                   start_id;
  };

  struct header_t;
  friend class detail::interrupt_mask;
  friend class detail::comm_stats;

 public:
  comm(int *argc, char ***argv);

  // TODO:  Add way to detect if this MPI_Comm is already open. E.g., static
  // map<MPI_Comm, impl*>
  comm(MPI_Comm comm);

  ~comm();

  /**
   * @brief Prints a welcome message with configuration details.
   *
   */
  void welcome(std::ostream &os = std::cout);

  void stats_reset();
  void stats_print(const std::string &name = "", std::ostream &os = std::cout);

  //
  //  Asynchronous rpc interfaces.   Can be called inside OpenMP loop
  //

  template <typename AsyncFunction, typename... SendArgs>
  void async(int dest, AsyncFunction &&fn, const SendArgs &...args);

  template <typename AsyncFunction, typename... SendArgs>
  void async(int dest, AsyncFunction &&fn, const SendArgs &...args) const {
    const_cast<comm *>(this)->async(dest, fn, args...);
  }

  template <typename AsyncFunction, typename... SendArgs>
  void async_bcast(AsyncFunction &&fn, const SendArgs &...args);

  template <typename AsyncFunction, typename... SendArgs>
  void async_bcast(AsyncFunction &&fn, const SendArgs &...args) const {
    const_cast<comm *>(this)->async_bcast(fn, args...);
  }

  template <typename AsyncFunction, typename... SendArgs>
  void async_mcast(const std::vector<int> &dests, AsyncFunction &&fn,
                   const SendArgs &...args);

  template <typename AsyncFunction, typename... SendArgs>
  void async_mcast(const std::vector<int> &dests, AsyncFunction &&fn,
                   const SendArgs &...args) const {
    const_cast<comm *>(this)->async_mcast(dests, fn, args...);
  }

  //
  // Collective operations across all ranks.  Cannot be called inside OpenMP
  // region.

  void cf_barrier() const;

  void barrier();

  /**
   * @brief Full communicator barrier that can be called on const comm objects
   */
  void barrier() const { const_cast<comm *>(this)->barrier(); }

  void async_barrier();

  /**
   * @brief Asynchronous communicator barrier that can be called on const comm
   * objects
   */
  void async_barrier() const { const_cast<comm *>(this)->async_barrier(); }

  void local_progress();

  bool local_process_incoming();

  template <typename Function>
  void local_wait_until(Function fn);

  template <typename T>
  ygm_ptr<T> make_ygm_ptr(T &t);

  void register_pre_barrier_callback(const std::function<void()> &fn);

  template <typename T>
  [[deprecated("Use `ygm::sum` defined in ygm/detail/collective.hpp")]]
  T all_reduce_sum(const T &t) const;

  template <typename T>
  [[deprecated("Use `ygm::min` defined in ygm/detail/collective.hpp")]]
  T all_reduce_min(const T &t) const;

  template <typename T>
  [[deprecated("Use `ygm::max` defined in ygm/detail/collective.hpp")]]
  T all_reduce_max(const T &t) const;

  template <typename T, typename MergeFunction>
  [[deprecated("Use `ygm::all_reduce` defined in ygm/detail/collective.hpp")]]
  inline T all_reduce(const T &t, MergeFunction merge) const;

  //
  //  Communicator information
  //
  int size() const;
  int rank() const;

  MPI_Comm get_mpi_comm() const;

  const detail::layout &layout() const;

  const detail::comm_router &router() const;

  const detail::comm_stats &stats() const;

  /**
   * @brief Checks if current rank is rank 0
   *
   * @return bool indicating whether current rank is rank 0
   */
  bool rank0() const { return rank() == 0; }

  template <typename T>
  void mpi_send(const T &data, int dest, int tag, MPI_Comm comm) const;

  /**
   * @brief Send an MPI message over an unspecified MPI communicator
   *
   * @tparam T datatype being sent (must be serializable with cereal)
   * @param data Message contents to send
   * @param dest Rank to send data to
   * @param tag MPI tag to assign to message
   */
  template <typename T>
  void mpi_send(const T &data, int dest, int tag) const {
    mpi_send(data, dest, tag, m_comm_other);
  }

  template <typename T>
  T mpi_recv(int source, int tag, MPI_Comm comm) const;

  /**
   * @brief Receive an MPI message over an unspecified MPI communicator
   *
   * @tparam T datatype being received (must be serializable with cereal)
   * @param source Rank sending message
   * @param tag MPI tag to assign to message
   * @return Received message
   */
  template <typename T>
  T mpi_recv(int source, int tag) const {
    return mpi_recv<T>(source, tag, m_comm_other);
  }

  template <typename T>
  T mpi_bcast(const T &to_bcast, int root, MPI_Comm comm) const;

  /**
   * @brief Broadcast an MPI message over an unspecified MPI communicator
   *
   * @tparam Datatype to broadcast (must be serializable)
   * @param to_bcast Data being broadcast
   * @param root Rank message is being broadcast from
   * @return Data received from root
   */
  template <typename T>
  T mpi_bcast(const T &to_bcast, int root) const {
    return mpi_bcast(to_bcast, root, m_comm_other);
  }

  std::ostream &cout0() const;
  std::ostream &cerr0() const;
  std::ostream &cout() const;
  std::ostream &cerr() const;

  template <typename... Args>
  void cout(Args &&...args) const;

  template <typename... Args>
  void cerr(Args &&...args) const;

  template <typename... Args>
  void cout0(Args &&...args) const;

  template <typename... Args>
  void cerr0(Args &&...args) const;

  void enable_ygm_tracing();

  void disable_ygm_tracing();

  void enable_mpi_tracing();

  void disable_mpi_tracing();

  bool is_ygm_tracing_enabled() const;

  bool is_mpi_tracing_enabled() const;

  /**
   * @brief Set the log level to use in YGM
   *
   * @param level Log level to use. Possible values in order of increasing
   * verbosity are ygm::log_level::off, ygm::log_level::critical,
   * ygm::log_level::error, ygm::log_level::warn, ygm::log_level::info,
   * ygm::log_level::debug
   */
  void set_log_level(const ygm::log_level level) {
    m_logger.set_log_level(level);
  }

  /**
   * @brief Get the log level currently used in YGM
   *
   * @return Current log level
   */
  log_level get_log_level() { return m_logger.get_log_level(); }

  /**
   * @brief Set the logger target to use in YGM
   *
   * @param target Logger target to use. Possible values are
   * ygm::logger_target::file, ygm::logger_target::stdout, and
   * ygm::logger_target::stderr
   */
  void set_logger_target(const ygm::logger_target target) {
    m_logger.set_logger_target(target);
  }

  /**
   * @brief Get the logger target currently used in YGM
   *
   * @return Current logger target
   */
  logger_target get_logger_target() { return m_logger.get_logger_target(); }

  /**
   * @brief Add a message to the YGM logs
   *
   * @tparam Args... Variadic types to add to log
   * @param Minimum log level for logging message
   * @args Variadic arguments add to log
   *
   * \code{cpp}
   *    int var = 6;
   *    world.log(ygm::log_level::info, "This is my var: ", var);
   * \endcode
   */
  template <typename... Args>
  void log(const ygm::log_level level, Args &&...args) const {
    m_logger.log(level, args...);
  }

  /**
   * @brief Add a message to the YGM logs written to multiple targets
   *
   * @tparam Args... Variadic types to add to log
   * @param targets Vector of targets to write logs to
   * @param Minimum log level for logging message
   * @args Variadic arguments add to log
   */
  template <typename... Args>
  void log(const std::vector<logger_target> &targets,
           const ygm::log_level              level, Args &&...args) const {
    m_logger.log(targets, level, args...);
  }

  /**
   * @brief Set the log location to use when logging to files
   *
   * @param s Log location
   */
  template <typename StringType>
  void set_log_location(const StringType &s);

  /**
   * @brief Set the log location to use when logging to files
   *
   * @param p Log location
   */
  void set_log_location(std::filesystem::path p);

  // Private member functions
 private:
  void comm_setup(MPI_Comm comm);

  size_t pack_header(ygm::detail::byte_vector &packed, const int dest,
                     size_t size);

  std::pair<uint64_t, uint64_t> barrier_reduce_counts();

  void flush_next_send(std::deque<int> &dest_queue);

  void flush_send_buffer(int dest);

  void handle_completed_send(mpi_isend_request &req_buffer);

  void check_completed_sends();

  void check_if_production_halt_required();

  void flush_all_local_and_process_incoming();

  void flush_to_capacity();

  void post_new_irecv(std::shared_ptr<ygm::detail::byte_vector> &recv_buffer);

  template <typename Lambda, typename... PackArgs>
  size_t pack_lambda(ygm::detail::byte_vector &packed, Lambda &&l,
                     const PackArgs &...args);

  template <typename Lambda, typename... PackArgs>
  void pack_lambda_broadcast(Lambda &&l, const PackArgs &...args);

  template <typename Lambda, typename RemoteLogicLambda, typename... PackArgs>
  size_t pack_lambda_generic(ygm::detail::byte_vector &packed, Lambda &&l,
                             RemoteLogicLambda rll, const PackArgs &...args);

  void queue_message_bytes(const ygm::detail::byte_vector &packed,
                           const int                       dest);

  void handle_next_receive(std::shared_ptr<ygm::detail::byte_vector> &buffer,
                           const size_t buffer_size, const uint32_t from_rank);

  bool process_receive_queue();

  bool priv_barrier(bool is_full);

  comm() = delete;

  comm(const comm &c) = delete;

  // Private member variables
 private:
  std::shared_ptr<detail::mpi_init_finalize> pimpl_if;

  MPI_Comm m_comm_async;
  MPI_Comm m_comm_barrier;
  MPI_Comm m_comm_other;

  std::vector<ygm::detail::byte_vector> m_vec_send_buffers;

  size_t          m_send_local_buffer_bytes = 0;
  std::deque<int> m_send_local_dest_queue;
  size_t          m_send_remote_buffer_bytes = 0;
  std::deque<int> m_send_remote_dest_queue;

  std::deque<mpi_irecv_request>                          m_recv_queue;
  std::deque<mpi_isend_request>                          m_send_queue;
  std::vector<std::shared_ptr<ygm::detail::byte_vector>> m_free_send_buffers;

  size_t m_pending_isend_bytes = 0;

  std::deque<std::function<void()>> m_pre_barrier_callbacks;

  bool m_enable_interrupts = true;

  uint64_t m_recv_count = 0;
  uint64_t m_send_count = 0;

  bool m_in_process_receive_queue = false;

  detail::comm_stats             m_stats;
  const detail::layout           m_layout;
  const detail::comm_environment config = detail::comm_environment(m_layout);
  detail::comm_router            m_router;
  detail::tracer                 m_tracer =
      detail::tracer(m_layout.size(), m_layout.rank(), config.trace_path);
  bool m_trace_ygm = config.trace_ygm;
  bool m_trace_mpi = config.trace_mpi;

  detail::logger m_logger;

  detail::lambda_map<void (*)(comm *, cereal::YGMInputArchive *), uint16_t>
      m_lambda_map;
};

}  // end namespace ygm

#include <ygm/detail/comm.ipp>
