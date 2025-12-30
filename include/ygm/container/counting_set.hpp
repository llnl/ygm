// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <ygm/comm.hpp>
#include <ygm/container/container_traits.hpp>
#include <ygm/container/detail/base_contains.hpp>
#include <ygm/container/detail/base_count.hpp>
#include <ygm/container/detail/base_iteration.hpp>
#include <ygm/container/detail/base_misc.hpp>
#include <ygm/container/map.hpp>
#include <ygm/detail/ygm_ptr.hpp>

namespace ygm::container {

/**
 * @brief `ygm::container::map` that is specialized for counting occurrences of
 * items in a stream.
 *
 * @details Adds a local cache of objects to reduce sends of
 * frequently-occurring items
 */
template <typename Key>
class counting_set
    : public detail::base_count<counting_set<Key>, std::tuple<Key, size_t>>,
      public detail::base_contains<counting_set<Key>, std::tuple<Key, size_t>>,
      public detail::base_misc<counting_set<Key>, std::tuple<Key, size_t>>,
      public detail::base_iterators<counting_set<Key>>,
      public detail::base_iteration_key_value<counting_set<Key>,
                                              std::tuple<Key, size_t>> {
  friend struct detail::base_misc<counting_set<Key>, std::tuple<Key, size_t>>;

  using internal_container_type = map<Key, size_t>;

 public:
  using self_type      = counting_set<Key>;
  using mapped_type    = size_t;
  using key_type       = Key;
  using size_type      = size_t;
  using for_all_args   = std::tuple<Key, size_t>;
  using container_type = ygm::container::counting_set_tag;
  using iterator       = typename internal_container_type::iterator;
  using const_iterator = typename internal_container_type::const_iterator;

  const size_type count_cache_size = 1024 * 1024;

  /**
   * @brief counting_set constructor
   *
   * @param comm Communicator to use for communication
   */
  counting_set(ygm::comm &comm)
      : m_comm(comm), pthis(this), m_map(comm), partitioner(m_map.partitioner) {
    m_comm.log(log_level::info, "Creating ygm::container::counting_set");
    pthis.check(m_comm);
    m_count_cache.resize(count_cache_size, {key_type(), -1});
  }

  counting_set() = delete;

  /**
   * @brief counting_set constructor from std::initializer_list of values
   *
   * @param comm Communicator to use for communication
   * @param l Initializer list of values to put in counting_set
   * @details Initializer list is assumed to be replicated on all ranks.
   */
  counting_set(ygm::comm &comm, std::initializer_list<Key> l)
      : m_comm(comm), pthis(this), m_map(comm), partitioner(m_map.partitioner) {
    m_comm.log(log_level::info, "Creating ygm::container::counting_set");
    pthis.check(m_comm);
    m_count_cache.resize(count_cache_size, {key_type(), -1});
    if (m_comm.rank0()) {
      for (const Key &i : l) {
        async_insert(i);
      }
    }
    m_comm.barrier();
  }

  /**
   * @brief Construct counting_set by counting items in existing STL container
   *
   * @tparam STLContainer Existing container type
   * @param comm Communicator to use for communication
   * @param cont STL container containing values to count
   */
  template <typename STLContainer>
  counting_set(ygm::comm &comm, const STLContainer &cont)
    requires detail::STLContainer<STLContainer> &&
                 std::convertible_to<typename STLContainer::value_type, Key>
      : m_comm(comm), pthis(this), m_map(comm), partitioner(comm) {
    m_comm.log(log_level::info, "Creating ygm::container::counting_set");
    pthis.check(m_comm);
    m_count_cache.resize(count_cache_size, {key_type(), -1});
    for (const Key &i : cont) {
      this->async_insert(i);
    }
    m_comm.barrier();
  }

  /**
   * @brief Construct counting_set by counting items in existing YGM container
   *
   * @tparam YGMContainer Existing container type
   * @param comm Communicator to use for communication
   * @param yc YGM container containing values to count
   */
  template <typename YGMContainer>
  counting_set(ygm::comm &comm, const YGMContainer &yc)
    requires detail::HasForAll<YGMContainer> &&
                 detail::SingleItemTuple<typename YGMContainer::for_all_args>
      : m_comm(comm), pthis(this), m_map(comm), partitioner(comm) {
    m_comm.log(log_level::info, "Creating ygm::container::counting_set");
    pthis.check(m_comm);
    m_count_cache.resize(count_cache_size, {key_type(), -1});
    yc.for_all([this](const Key &value) { this->async_insert(value); });

    m_comm.barrier();
  }

  ~counting_set() {
    m_comm.barrier();
    m_comm.log(log_level::info, "Destroying ygm::container::counting_set");
  }

  counting_set(const self_type &other)
      : m_comm(other.comm()),
        pthis(this),
        m_count_cache(other.m_count_cache),
        m_cache_empty(other.m_cache_empty),
        m_map(other.m_map) {
    m_comm.log(log_level::info, "Copying ygm::container::counting_set");
    pthis.check(m_comm);
  }

  counting_set(self_type &&other)
      : m_comm(other.comm()),
        pthis(this),
        m_count_cache(std::move(other.m_count_cache)),
        m_cache_empty(other.m_cache_empty),
        m_map(std::move(other.m_map)) {
    m_comm.log(log_level::info, "Moving ygm::container::counting_set");
    pthis.check(m_comm);
  }

  counting_set &operator=(const self_type &other) {
    m_comm.log(log_level::info,
               "Calling ygm::container::counting_set copy assignment operator");
    return *this = counting_set(other);
  }

  counting_set &operator=(self_type &&other) {
    m_comm.log(log_level::info,
               "Calling ygm::container::counting_set move assignment operator");
    std::swap(m_count_cache, other.m_count_cache);
    m_map = std::move(other.m_map);
    return *this;
  }

  /**
   * @brief Access to begin iterator of locally-held items
   *
   * @return Local iterator to beginning of items held by process.
   * @details Does not call `barrier()`.
   */
  iterator local_begin() { return m_map.begin(); }

  /**
   * @brief Access to begin const_iterator of locally-held items for const
   * counting_set
   *
   * @return Local const iterator to beginning of items held by process.
   * @details Does not call `barrier()`.
   */
  const_iterator local_begin() const { return m_map.cbegin(); }

  /**
   * @brief Access to begin const_iterator of locally-held items for const
   * counting_set
   *
   * @return Local const iterator to beginning of items held by process.
   * @details Does not call `barrier()`.
   */
  const_iterator local_cbegin() const { return m_map.cbegin(); }

  /**
   * @brief Access to end iterator of locally-held items
   *
   * @return Local iterator to end of items held by process.
   * @details Does not call `barrier()`.
   */
  iterator local_end() { return m_map.end(); }

  /**
   * @brief Access to end const_iterator of locally-held items for const
   * counting_set
   *
   * @return Local const iterator to ending of items held by process.
   * @details Does not call `barrier()`.
   */
  const_iterator local_end() const { return m_map.cend(); }

  /**
   * @brief Access to end const_iterator of locally-held items for const
   * counting_set
   *
   * @return Local const iterator to ending of items held by process.
   * @details Does not call `barrier()`.
   */
  const_iterator local_cend() const { return m_map.cend(); }

  /**
   * @brief Asynchronously insert an item for counting
   *
   * @param key Item to count
   * @details Inserts item into local cache before sending count to remote
   * location
   */
  void async_insert(const key_type &key) { cache_insert(key); }

  /**
   * @brief Execute a functor on every locally-held item and count
   *
   * @tparam Function functor type
   * @param fn Functor to execute on items and counts
   */
  template <typename Function>
  void local_for_all(Function &&fn) {
    m_map.local_for_all(std::forward<Function>(fn));
  }

  /**
   * @brief Execute a functor on every locally-held item and count for a const
   * container
   *
   * @tparam Function functor type
   * @param fn Functor to execute on items and counts
   */
  template <typename Function>
  void local_for_all(Function &&fn) const {
    m_map.local_for_all(std::forward<Function>(fn));
  }

  /**
   * @brief Clear the local storage of the counting_set
   */
  void local_clear() {  // What to do here
    m_map.local_clear();
    clear_cache();
  }

  using detail::base_misc<counting_set<Key>, for_all_args>::clear;

  /**
   * @brief Clear the global storage of the counting_set
   */
  void clear() {
    local_clear();
    m_comm.barrier();
  }

  /**
   * @brief Get the number of items held locally
   *
   * @return Number of locally-held items
   */
  size_t local_size() const { return m_map.local_size(); }

  /**
   * @brief Get the total number of times a locally-held item has been counted
   * so far
   *
   * @return Number of times a locally-held item has been counted
   * @details Counts can be inaccurate before a `barrier()` due to items still
   * being cached on other processes or waiting to be sent in `ygm::comm`
   * buffers.
   */
  mapped_type local_count(const key_type &key) const {
    auto        vals = m_map.local_get(key);
    mapped_type local_count{0};
    for (auto v : vals) {
      local_count += v;
    }
    return local_count;
  }

  /**
   * @brief Check if a locally-held item exists
   *
   * @param val Value to check for
   * @return true if value exists locally, false otherwise
   */
  bool local_contains(const key_type &key) const {
    return m_map.local_contains(key);
  }

  /**
   * @brief Count the total number of items counted
   *
   * @return Sum of all item counts
   */
  mapped_type count_all() {
    mapped_type local_count{0};
    local_for_all(
        [&local_count]([[maybe_unused]]const auto &key, auto &value) { local_count += value; });
    return ::ygm::sum(local_count, m_map.comm());
  }

  // bool is_mine(const key_type &key) const { return m_map.is_mine(key); }

  /**
   * @brief Gather the k "largest" item-count pairs according to provided
   * comparison function
   *
   * @tparam Compare Type of comparison operator
   * @param k Number of item-count pairs to gather
   * @param comp Comparison function for identifying elements to gather
   * @return vector of largest item-count pairs
   */
  template <typename CompareFunction>
  std::vector<std::pair<key_type, mapped_type>> topk(size_t          k,
                                                     CompareFunction cfn) {
    return m_map.topk(k, cfn);
  }

  // template <typename STLKeyContainer>
  // std::map<key_type, mapped_type> all_gather(const STLKeyContainer &keys) {
  //   return m_map.all_gather(keys);
  // }

  /**
   * @brief Collective operation to look up item counts from each rank
   *
   * @param keys Keys local rank wants to collect counts for
   * @return `std::map` of provided keys and their counts
   */
  std::map<key_type, mapped_type> gather_keys(
      const std::vector<key_type> &keys) {
    return m_map.gather_keys(keys);
  }

  /**
   * @brief Access to the ygm_ptr used by the container
   *
   * @return `ygm_ptr` used by the container when identifying itself in `async`
   * calls on the `ygm::comm`
   */
  typename ygm::ygm_ptr<self_type> get_ygm_ptr() const { return pthis; }

  /**
   * @brief Serialize counting set contents to collection of files
   *
   * @param fname Filename prefix to create names for files used by each rank
   */
  void serialize(const std::string &fname) { m_map.serialize(fname); }

  /**
   * @brief Deserialize counting set contents from collection of files
   *
   * @param fname Filename prefix to create names for files used by each rank
   */
  void deserialize(const std::string &fname) { m_map.deserialize(fname); }

 private:
  /**
   * @brief Remove key from local cache and distributed map
   *
   * @param key Key to erase from cache
   * @details This function erases the key from the underlying
   * `ygm::container::map` and the local cache, but the key may still be found
   * in caches on other ranks.
   */
  void cache_erase(const key_type &key) {
    size_t slot = detail::hash<key_type>{}(key) % count_cache_size;
    if (m_count_cache[slot].second != -1 && m_count_cache[slot].first == key) {
      // Key was cached, clear cache
      m_count_cache[slot].second = -1;
      m_count_cache[slot].first  = key_type();
    }
    m_map.async_erase(key);
  }

  /**
   * @brief Insert key into local cache. If key already exists, increment count
   * in local cache. If other key exists in desired cache slot, flush cached
   * value by sending to count to distributed `ygm::container::map`.
   */
  void cache_insert(const key_type &key) {
    if (m_cache_empty) {
      m_cache_empty = false;
      m_map.comm().register_pre_barrier_callback(
          [this]() { this->count_cache_flush_all(); });
    }
    size_t slot = detail::hash<key_type>{}(key) % count_cache_size;
    if (m_count_cache[slot].second == -1) {
      m_count_cache[slot].first  = key;
      m_count_cache[slot].second = 1;
    } else {
      // flush slot, fill with key
      YGM_ASSERT_DEBUG(m_count_cache[slot].second > 0);
      if (m_count_cache[slot].first == key) {
        m_count_cache[slot].second++;
      } else {
        count_cache_flush(slot);
        YGM_ASSERT_DEBUG(m_count_cache[slot].second == -1);
        m_count_cache[slot].first  = key;
        m_count_cache[slot].second = 1;
      }
    }
    if (m_count_cache[slot].second == std::numeric_limits<int32_t>::max()) {
      count_cache_flush(slot);
    }
  }

  /**
   * @brief Flush a slot in the local cache to the underlying
   * `ygm::container::map`
   *
   * @param slot Cache slot to flush
   */
  void count_cache_flush(size_t slot) {
    auto key          = m_count_cache[slot].first;
    auto cached_count = m_count_cache[slot].second;
    YGM_ASSERT_DEBUG(cached_count > 0);
    m_map.async_visit(
        key,
        []([[maybe_unused]] const key_type &key, size_t &count,
           int32_t to_add) { count += to_add; },
        cached_count);
    m_count_cache[slot].first  = key_type();
    m_count_cache[slot].second = -1;
  }

  /**
   * @brief Flush all cached counts
   */
  void count_cache_flush_all() {
    if (!m_cache_empty) {
      for (size_t i = 0; i < m_count_cache.size(); ++i) {
        if (m_count_cache[i].second > 0) {
          count_cache_flush(i);
        }
      }
      m_cache_empty = true;
    }
  }

  /**
   * @brief Clear all local cache contents
   */
  void clear_cache() {
    for (size_t i = 0; i < m_count_cache.size(); ++i) {
      m_count_cache[i].first  = key_type();
      m_count_cache[i].second = -1;
    }
    m_cache_empty = true;
  }

  ygm::comm                           &m_comm;
  typename ygm::ygm_ptr<self_type>     pthis;
  std::vector<std::pair<Key, int32_t>> m_count_cache;
  bool                                 m_cache_empty = true;
  internal_container_type              m_map;

 public:
  detail::hash_partitioner<detail::hash<key_type>> partitioner;
};

}  // namespace ygm::container
