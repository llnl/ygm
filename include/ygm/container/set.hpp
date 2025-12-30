// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <boost/unordered/unordered_flat_set.hpp>
#include <ygm/container/container_traits.hpp>
#include <ygm/container/detail/base_async_contains.hpp>
#include <ygm/container/detail/base_async_erase.hpp>
#include <ygm/container/detail/base_async_insert.hpp>
#include <ygm/container/detail/base_async_insert_contains.hpp>
#include <ygm/container/detail/base_batch_erase.hpp>
#include <ygm/container/detail/base_contains.hpp>
#include <ygm/container/detail/base_count.hpp>
#include <ygm/container/detail/base_iteration.hpp>
#include <ygm/container/detail/base_iterators.hpp>
#include <ygm/container/detail/base_misc.hpp>
#include <ygm/container/detail/hash_partitioner.hpp>

namespace ygm::container {

template <typename Value>
class set
    : public detail::base_async_insert_value<set<Value>, std::tuple<Value>>,
      public detail::base_async_erase_key<set<Value>, std::tuple<Value>>,
      public detail::base_batch_erase_key<set<Value>, std::tuple<Value>>,
      public detail::base_async_contains<set<Value>, std::tuple<Value>>,
      public detail::base_async_insert_contains<set<Value>, std::tuple<Value>>,
      public detail::base_contains<set<Value>, std::tuple<Value>>,
      public detail::base_count<set<Value>, std::tuple<Value>>,
      public detail::base_misc<set<Value>, std::tuple<Value>>,
      public detail::base_iterators<set<Value>>,
      public detail::base_iteration_value<set<Value>, std::tuple<Value>> {
  friend struct detail::base_misc<set<Value>, std::tuple<Value>>;

  using local_container_type =
      boost::unordered::unordered_flat_set<Value, detail::hash<Value>>;

 public:
  using self_type      = set<Value>;
  using value_type     = Value;
  using size_type      = size_t;
  using for_all_args   = std::tuple<Value>;
  using container_type = ygm::container::set_tag;
  using iterator       = typename local_container_type::iterator;
  using const_iterator = typename local_container_type::const_iterator;

  /**
   * @brief Set constructor
   *
   * @param comm Communicator to use for communication
   */
  set(ygm::comm &comm)
      : m_comm(comm),
        pthis(this),
        partitioner(comm, detail::hash<value_type>()) {
    m_comm.log(log_level::info, "Creating ygm::container::set");
    pthis.check(m_comm);
  }

  /**
   * @brief Set constructor from std::initializer_list of sets
   *
   * @param comm Communicator to use for communication
   * @param l Initializer list of values to put in set
   * @details Initializer list is assumed to be replicated on all ranks.
   */
  set(ygm::comm &comm, std::initializer_list<Value> l)
      : m_comm(comm), pthis(this), partitioner(comm) {
    m_comm.log(log_level::info, "Creating ygm::container::set");
    pthis.check(m_comm);
    if (m_comm.rank0()) {
      for (const Value &i : l) {
        this->async_insert(i);
      }
    }
    m_comm.barrier();
  }

  /**
   * @brief Construct set from existing STL container
   *
   * @tparam T Existing container type
   * @param comm Communicator to use for communication
   * @param cont STL container containing values to put in set
   */
  template <typename STLContainer>
  set(ygm::comm &comm, const STLContainer &cont)
    requires detail::STLContainer<STLContainer> &&
                 std::convertible_to<typename STLContainer::value_type, Value>
      : m_comm(comm), pthis(this), partitioner(comm) {
    m_comm.log(log_level::info, "Creating ygm::container::set");
    pthis.check(m_comm);

    for (const Value &i : cont) {
      this->async_insert(i);
    }
    m_comm.barrier();
  }

  /**
   * @brief Construct set from existing YGM container of values
   *
   * @tparam T Existing container type
   * @param comm Communicator to use for communication
   * @param yc YGM container of values to put in set.
   */
  template <typename YGMContainer>
  set(ygm::comm &comm, const YGMContainer &yc)
    requires detail::HasForAll<YGMContainer> &&
                 detail::SingleItemTuple<typename YGMContainer::for_all_args>
      : m_comm(comm), pthis(this), partitioner(comm) {
    m_comm.log(log_level::info, "Creating ygm::container::set");
    pthis.check(m_comm);

    yc.for_all([this](const Value &value) { this->async_insert(value); });

    m_comm.barrier();
  }

  ~set() {
    m_comm.log(log_level::info, "Destroying ygm::container::set");
    m_comm.barrier();
  }

  set() = delete;

  set(const self_type &other)
      : m_comm(other.comm()),
        pthis(this),
        m_local_set(other.m_local_set),
        partitioner(other.comm()) {
    m_comm.log(log_level::info, "Creating ygm::container::set");
    pthis.check(m_comm);
  }

  set(self_type &&other) noexcept
      : m_comm(other.comm()),
        pthis(this),
        m_local_set(std::move(other.m_local_set)),
        partitioner(other.partitioner) {
    m_comm.log(log_level::info, "Creating ygm::container::set");
    pthis.check(m_comm);
  }

  set &operator=(const self_type &other) { return *this = set(other); }

  set &operator=(self_type &&other) noexcept {
    std::swap(m_local_set, other.m_local_set);
    return *this;
  }

  /**
   * @brief Access to begin iterator of locally-held items
   *
   * @return Local iterator to beginning of items held by process.
   * @details Does not call `barrier()`.
   */
  iterator local_begin() { return m_local_set.begin(); }

  /**
   * @brief Access to begin const_iterator of locally-held items for const set
   *
   * @return Local const iterator to beginning of items held by process.
   * @details Does not call `barrier()`.
   */
  const_iterator local_begin() const { return m_local_set.cbegin(); }

  /**
   * @brief Access to begin const_iterator of locally-held items for const set
   *
   * @return Local const iterator to beginning of items held by process.
   * @details Does not call `barrier()`.
   */
  const_iterator local_cbegin() const { return m_local_set.cbegin(); }

  /**
   * @brief Access to end iterator of locally-held items
   *
   * @return Local iterator to ending of items held by process.
   * @details Does not call `barrier()`.
   */
  iterator local_end() { return m_local_set.end(); }

  /**
   * @brief Access to end const_iterator of locally-held items for const set
   *
   * @return Local const iterator to ending of items held by process.
   * @details Does not call `barrier()`.
   */
  const_iterator local_end() const { return m_local_set.cend(); }

  /**
   * @brief Access to end const_iterator of locally-held items for const set
   *
   * @return Local const iterator to ending of items held by process.
   * @details Does not call `barrier()`.
   */
  const_iterator local_cend() const { return m_local_set.cend(); }

  using detail::base_batch_erase_key<set<Value>, for_all_args>::erase;

  /**
   * @brief Insert a value into local storage
   *
   * @param val Value to store
   */
  void local_insert(const value_type &val) { m_local_set.insert(val); }

  /**
   * @brief Erase value from local storage
   *
   * @param val Value to erase from local storage
   */
  void local_erase(const value_type &val) { m_local_set.erase(val); }

  /**
   * @brief Clear local storage
   */
  void local_clear() { m_local_set.clear(); }

  /**
   * @brief Count the number of times a value is found locally
   *
   * @return Number of local occurrences of `val`
   */
  size_t local_count(const value_type &val) const {
    return m_local_set.count(val);
  }

  /**
   * @brief Check if a value exists locally
   *
   * @param val Value to check for
   * @return True if value exists locally, false otherwise
   */
  bool local_contains(const value_type &val) const {
    return m_local_set.contains(val);
  }

  /**
   * @brief Get the number of elements stored on the local process.
   *
   * @return Local size of set
   */
  size_t local_size() const { return m_local_set.size(); }

  /**
   * @brief Execute a functor on every locally-held value
   *
   * @tparam Function functor type
   * @param fn Functor to execute on values
   */
  template <typename Function>
  void local_for_all(Function &&fn) {
    std::for_each(m_local_set.begin(), m_local_set.end(),
                  std::forward<Function>(fn));
  }

  /**
   * @brief `local_for_all` for `const` containers
   *
   * @tparam Function functor type
   * @param fn Functor to execute on values
   * @details `const` references to values are provided to `fn`
   */
  template <typename Function>
  void local_for_all(Function &&fn) const {
    std::for_each(m_local_set.cbegin(), m_local_set.cend(),
                  std::forward<Function>(fn));
  }

  /**
   * @brief Serialize a set to a collection of files to be read back in later
   *
   * @param fname Filename prefix to create filename used by every rank from
   */
  void serialize([[maybe_unused]] const std::string &fname) {}

  /**
   * @brief Deserialize a set from files
   *
   * @param fname Filename prefix to create filename used by every rank from
   * @details Currently requires the number of ranks deserializing a bag to be
   * the same as was used for serialization.
   */
  void deserialize([[maybe_unused]] const std::string &fname) {}

  /**
   * @brief Swap elements held locally between sets
   *
   * @param other Set to swap elements with
   */
  void local_swap(self_type &other) { m_local_set.swap(other.m_local_set); }

 private:
  ygm::comm                       &m_comm;
  typename ygm::ygm_ptr<self_type> pthis;
  local_container_type             m_local_set;

 public:
  detail::hash_partitioner<detail::hash<value_type>> partitioner;
};

}  // namespace ygm::container
