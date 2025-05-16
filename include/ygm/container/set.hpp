// Copyright 2019-2021 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <set>
#include <ygm/container/container_traits.hpp>
#include <ygm/container/detail/base_async_contains.hpp>
#include <ygm/container/detail/base_async_erase.hpp>
#include <ygm/container/detail/base_async_insert.hpp>
#include <ygm/container/detail/base_async_insert_contains.hpp>
#include <ygm/container/detail/base_batch_erase.hpp>
#include <ygm/container/detail/base_count.hpp>
#include <ygm/container/detail/base_iteration.hpp>
#include <ygm/container/detail/base_misc.hpp>
#include <ygm/container/detail/hash_partitioner.hpp>

namespace ygm::container {

template <typename Value>
class multiset
    : public detail::base_async_insert_value<multiset<Value>,
                                             std::tuple<Value>>,
      public detail::base_async_erase_key<multiset<Value>, std::tuple<Value>>,
      public detail::base_batch_erase_key<multiset<Value>, std::tuple<Value>>,
      public detail::base_async_contains<multiset<Value>, std::tuple<Value>>,
      public detail::base_async_insert_contains<multiset<Value>,
                                                std::tuple<Value>>,
      public detail::base_count<multiset<Value>, std::tuple<Value>>,
      public detail::base_misc<multiset<Value>, std::tuple<Value>>,
      public detail::base_iteration_value<multiset<Value>, std::tuple<Value>> {
  friend class detail::base_misc<multiset<Value>, std::tuple<Value>>;

 public:
  using self_type      = multiset<Value>;
  using value_type     = Value;
  using size_type      = size_t;
  using for_all_args   = std::tuple<Value>;
  using container_type = ygm::container::set_tag;

  multiset(ygm::comm &comm)
      : m_comm(comm), pthis(this), partitioner(comm, std::hash<value_type>()) {
    m_comm.log(log_level::info, "Creating ygm::container::multiset");
    pthis.check(m_comm);
  }

  multiset(ygm::comm &comm, std::initializer_list<Value> l)
      : m_comm(comm), pthis(this), partitioner(comm) {
    m_comm.log(log_level::info, "Creating ygm::container::multiset");
    pthis.check(m_comm);
    if (m_comm.rank0()) {
      for (const Value &i : l) {
        async_insert(i);
      }
    }

    m_comm.barrier();
  }

  template <typename STLContainer>
  multiset(ygm::comm &comm, const STLContainer &cont)
    requires detail::STLContainer<STLContainer> &&
                 std::convertible_to<typename STLContainer::value_type, Value>
      : m_comm(comm), pthis(this), partitioner(comm) {
    m_comm.log(log_level::info, "Creating ygm::container::multiset");
    pthis.check(m_comm);

    for (const Value &i : cont) {
      this->async_insert(i);
    }

    m_comm.barrier();
  }

  template <typename YGMContainer>
  multiset(ygm::comm          &comm,
           const YGMContainer &yc)
    requires detail::HasForAll<YGMContainer> &&
                 detail::SingleItemTuple<
                     typename YGMContainer::for_all_args>  //&&
      // std::same_as<typename TYGMContainer::for_all_args, std::tuple<Value>>
      : m_comm(comm), pthis(this), partitioner(comm) {
    m_comm.log(log_level::info, "Creating ygm::container::multiset");
    pthis.check(m_comm);

    yc.for_all([this](const Value &value) { this->async_insert(value); });

    m_comm.barrier();
  }

  ~multiset() {
    m_comm.log(log_level::info, "Destroying ygm::container::multiset");
    m_comm.barrier();
  }

  multiset() = delete;

  multiset(const self_type &other)
      : m_comm(other.comm()),
        pthis(this),
        partitioner(other.comm()),
        m_local_set(other.m_local_set) {
    m_comm.log(log_level::info, "Copying ygm::container::multiset");
    pthis.check(m_comm);
  }

  multiset(self_type &&other) noexcept
      : m_comm(other.comm()),
        pthis(this),
        partitioner(other.partitioner),
        m_local_set(std::move(other.m_local_set)) {
    m_comm.log(log_level::info, "Moving ygm::container::multiset");
    pthis.check(m_comm);
  }

  multiset &operator=(const self_type &other) {
    m_comm.log(log_level::info,
               "Calling ygm::container::multiset copy assignment operator");
    return *this = multiset(other);
  }

  multiset &operator=(self_type &&other) noexcept {
    m_comm.log(log_level::info,
               "Calling ygm::container::multiset move assignment operator");
    std::swap(m_local_set, other.m_local_set);
    return *this;
  }

  void local_insert(const value_type &val) { m_local_set.insert(val); }

  void local_erase(const value_type &val) { m_local_set.erase(val); }

  void local_clear() { m_local_set.clear(); }

  size_t local_count(const value_type &val) const {
    return m_local_set.count(val);
  }

  size_t local_size() const { return m_local_set.size(); }

  template <typename Function>
  void local_for_all(Function fn) {
    std::for_each(m_local_set.begin(), m_local_set.end(), fn);
  }

  template <typename Function>
  void local_for_all(Function fn) const {
    std::for_each(m_local_set.cbegin(), m_local_set.cend(), fn);
  }

  void serialize(const std::string &fname) {}

  void deserialize(const std::string &fname) {}

  detail::hash_partitioner<std::hash<value_type>> partitioner;

  void local_swap(self_type &other) { m_local_set.swap(other.m_local_set); }

 private:
  ygm::comm                       &m_comm;
  std::multiset<value_type>        m_local_set;
  typename ygm::ygm_ptr<self_type> pthis;
};

template <typename Value>
class set
    : public detail::base_async_insert_value<set<Value>, std::tuple<Value>>,
      public detail::base_async_erase_key<set<Value>, std::tuple<Value>>,
      public detail::base_batch_erase_key<set<Value>, std::tuple<Value>>,
      public detail::base_async_contains<set<Value>, std::tuple<Value>>,
      public detail::base_async_insert_contains<set<Value>, std::tuple<Value>>,
      public detail::base_count<set<Value>, std::tuple<Value>>,
      public detail::base_misc<set<Value>, std::tuple<Value>>,
      public detail::base_iteration_value<set<Value>, std::tuple<Value>> {
  friend class detail::base_misc<set<Value>, std::tuple<Value>>;

 public:
  using self_type      = set<Value>;
  using value_type     = Value;
  using size_type      = size_t;
  using for_all_args   = std::tuple<Value>;
  using container_type = ygm::container::set_tag;

  set(ygm::comm &comm)
      : m_comm(comm), pthis(this), partitioner(comm, std::hash<value_type>()) {
    m_comm.log(log_level::info, "Creating ygm::container::set");
    pthis.check(m_comm);
  }

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

  template <typename YGMContainer>
  set(ygm::comm          &comm,
      const YGMContainer &yc)
    requires detail::HasForAll<YGMContainer> &&
                 detail::SingleItemTuple<
                     typename YGMContainer::for_all_args>  //&&
      // std::same_as<typename TYGMContainer::for_all_args, std::tuple<Value>>
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
        partitioner(other.comm()),
        m_local_set(other.m_local_set) {
    m_comm.log(log_level::info, "Creating ygm::container::set");
    pthis.check(m_comm);
  }

  set(self_type &&other) noexcept
      : m_comm(other.comm()),
        pthis(this),
        partitioner(other.partitioner),
        m_local_set(std::move(other.m_local_set)) {
    m_comm.log(log_level::info, "Creating ygm::container::set");
    pthis.check(m_comm);
  }

  set &operator=(const self_type &other) { return *this = set(other); }

  set &operator=(self_type &&other) noexcept {
    std::swap(m_local_set, other.m_local_set);
    return *this;
  }

  using detail::base_batch_erase_key<set<Value>, for_all_args>::erase;

  void local_insert(const value_type &val) { m_local_set.insert(val); }

  void local_erase(const value_type &val) { m_local_set.erase(val); }

  void local_clear() { m_local_set.clear(); }

  size_t local_count(const value_type &val) const {
    return m_local_set.count(val);
  }

  size_t local_size() const { return m_local_set.size(); }

  template <typename Function>
  void local_for_all(Function fn) {
    std::for_each(m_local_set.begin(), m_local_set.end(), fn);
  }

  template <typename Function>
  void local_for_all(Function fn) const {
    std::for_each(m_local_set.cbegin(), m_local_set.cend(), fn);
  }

  void serialize(const std::string &fname) {}

  void deserialize(const std::string &fname) {}

  detail::hash_partitioner<std::hash<value_type>> partitioner;

  void local_swap(self_type &other) { m_local_set.swap(other.m_local_set); }

 private:
  ygm::comm                       &m_comm;
  std::set<value_type>             m_local_set;
  typename ygm::ygm_ptr<self_type> pthis;
};

}  // namespace ygm::container
