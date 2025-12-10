// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <ygm/comm.hpp>
#include <ygm/container/container_traits.hpp>
#include <ygm/container/detail/base_misc.hpp>
#include <ygm/container/detail/work_queue_policy.hpp>
#include <ygm/detail/meta/functional.hpp>
#include <functional>

namespace ygm::container {

/**
 * @brief work queue container for YGM
 * 
 * @tparam Item Type of work items stored in queue
 * @tparam QueuePolicy Policy determining queue ordering (@see detail/work_queue_policy.hpp)
 * @tparam WorkLambda Lambda type for processing work items
 * 
 * @details Provides a work queue that processes items in FIFO, LIFO
 * order or priority order. Work is processed at barriers via registered callbacks.
 */

template <typename Item, typename QueuePolicy, typename WorkLambda>
class work_queue
    : public detail::base_misc<work_queue<Item, QueuePolicy, WorkLambda>,
                                           std::tuple<Item>> {
  
  friend struct detail::base_misc<work_queue<Item, QueuePolicy, WorkLambda>,
                                         std::tuple<Item>>;

 public:
  using self_type =         work_queue<Item, QueuePolicy, WorkLambda>;
  using ptr_type =                  typename ygm::ygm_ptr<self_type>;
  using value_type =                                            Item;
  using size_type =                                           size_t;
  using queue_type =                typename QueuePolicy::queue_type;

  work_queue() = delete;

  /**
   * @brief work_queue constructor
   * 
   * @param comm Communicator to use for callback registration
   * @param work_fn Lambda to execute on each work item during processing
   */
  work_queue(ygm::comm& comm, WorkLambda&& work_fn)
      : m_comm(comm),
        pthis(this),
        m_work_lambda(std::forward<WorkLambda>(work_fn)),
        m_callback_registered(false) {
    m_comm.log(log_level::info, "Creating ygm::container::work_queue");

    pthis.check(m_comm);
  }

  /**
   * @brief work_queue destructor
   * 
   * @details Asserts queue is empty before destruction to prevent items left
   * accidentally unprocessed. Call local_clear() explicitly to discard 
   * unfinished work before destruction.
   */
  ~work_queue() {
    m_comm.log(log_level::info, "Destroying ygm::container::work_queue");

    m_comm.barrier();
    YGM_ASSERT_RELEASE(local_size() == 0);
  }

  work_queue(self_type&& other) noexcept
      : m_comm(other.m_comm),
        pthis(this),
        m_local_queue(std::move(other.m_local_queue)),
        m_work_lambda(std::move(other.m_work_lambda)),
        m_callback_registered(false) {
    m_comm.log(log_level::info, "Moving ygm::container::work_queue");

    pthis.check(m_comm);
    other.m_callback_registered = false;

    if(local_has_work()) {
      register_processing_callback();
    }
  }

  work_queue& operator=(self_type&& other) noexcept {
    if (this == &other) return *this;

    m_comm.log(log_level::info,
               "Calling ygm::container::work_queue move assignment operator");

    m_local_queue = std::move(other.m_local_queue);
    m_callback_registered = false;
    other.m_callback_registered = false;

    if(local_has_work()) {
      register_processing_callback();
    }

    return *this;
  }

  /**
   * @brief Unsupported functions
   * 
   * @details Common YGM container functions that break under the 
   * execution model of the work_queue.
   */
  void size() = delete;
  void swap() = delete;
  work_queue(const self_type&) = delete;
  work_queue& operator=(const self_type&) = delete;


  /**
   * @brief Empties remaining items in global storage of work_queue.
   * May be discarded if local_clear and manual barrier the preferred method.
   */
  void clear() {
    local_clear();
    m_comm.barrier();
  }

  /**
   * @brief Insert a work item into the local queue
   * 
   * @param item Work item to insert
   * @details Registers processing callback on first insertion. Does not
   * initiate execution.
   */
  void local_insert(const Item& item) {
    QueuePolicy::push(m_local_queue, item);
    
    // Only register callback once per batch
    if (!m_callback_registered) {
      register_processing_callback();
    }
  }

  /**
   * @brief Process all pending work items in the local queue
   * 
   * @details Processes items according to queue policy.
   * Does not call comm.barrier().
   */
  void local_process_all() {
    while (!QueuePolicy::empty(m_local_queue)) {
      Item item = QueuePolicy::top(m_local_queue);
      QueuePolicy::pop(m_local_queue);
      ygm::meta::apply_optional(std::forward<WorkLambda>(m_work_lambda),
                                  std::make_tuple(pthis),
                                  std::forward_as_tuple(item));
    }
  }

  /**
   * @brief Check if there's pending work in the local queue
   * 
   * @return true if local queue has work, false otherwise
   */
  bool local_has_work() const {
    return !QueuePolicy::empty(m_local_queue);
  }

  /**
   * @brief Get the size of the local queue
   * 
   * @return Number of items in local queue
   */
  size_type local_size() const {
    return QueuePolicy::size(m_local_queue);
  }

  /**
   * @brief Clear the local queue without processing items
   * 
   * @details Use this if you want to discard pending work before destruction.
   * Does not call barrier().
   */
  void local_clear() {
    m_local_queue = queue_type{};
  }


 private:
  /**
   * @brief Register callback to process work at next barrier
   */
  void register_processing_callback() {    
    auto process_all_lambda = [this]() {
      this->local_process_all();
      this->m_callback_registered = false; // Reset for next batch
    };
    
    m_comm.register_pre_barrier_callback(process_all_lambda);
    m_callback_registered = true;
  }

  ygm::comm&                       m_comm;
  ptr_type                          pthis;
  queue_type                m_local_queue;
  WorkLambda                m_work_lambda;
  bool              m_callback_registered;
};

// Convenient type aliases
template <typename Item, typename WorkLambda>
using fifo_work_queue = work_queue<Item, detail::fifo_policy<Item>, WorkLambda>;

template <typename Item, typename WorkLambda>
using lifo_work_queue = work_queue<Item, detail::lifo_policy<Item>, WorkLambda>;

template <typename Item, typename Comp, typename WorkLambda>
using priority_work_queue = work_queue<Item, detail::priority_policy<Item, Comp>, WorkLambda>;

// Factory functions for convenient user instantiation
template <typename Item, typename WorkLambda>
auto make_fifo_work_queue(ygm::comm& comm, WorkLambda work_fn) {
  return fifo_work_queue<Item, WorkLambda>(comm, std::move(work_fn));
}

template <typename Item, typename WorkLambda>
auto make_lifo_work_queue(ygm::comm& comm, WorkLambda work_fn) {
  return lifo_work_queue<Item, WorkLambda>(comm, std::move(work_fn));
}

template <typename Item, typename Comp, typename WorkLambda>
auto make_priority_work_queue(ygm::comm& comm, WorkLambda work_fn) {
  return priority_work_queue<Item, Comp, WorkLambda>(comm, std::move(work_fn));
}

} // namespace ygm::container