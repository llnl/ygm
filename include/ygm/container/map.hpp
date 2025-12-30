// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <boost/unordered/unordered_flat_map.hpp>
#include <ygm/container/container_traits.hpp>
#include <ygm/container/detail/base_async_erase.hpp>
#include <ygm/container/detail/base_async_insert.hpp>
#include <ygm/container/detail/base_async_insert_or_assign.hpp>
#include <ygm/container/detail/base_async_reduce.hpp>
#include <ygm/container/detail/base_async_visit.hpp>
#include <ygm/container/detail/base_batch_erase.hpp>
#include <ygm/container/detail/base_contains.hpp>
#include <ygm/container/detail/base_count.hpp>
#include <ygm/container/detail/base_iteration.hpp>
#include <ygm/container/detail/base_iterators.hpp>
#include <ygm/container/detail/base_misc.hpp>
#include <ygm/container/detail/hash_partitioner.hpp>

namespace ygm::container {

template <typename Key, typename Value>
class map
    : public detail::base_async_insert_key_value<map<Key, Value>,
                                                 std::tuple<Key, Value>>,
      public detail::base_async_insert_or_assign<map<Key, Value>,
                                                 std::tuple<Key, Value>>,
      public detail::base_misc<map<Key, Value>, std::tuple<Key, Value>>,
      public detail::base_contains<map<Key, Value>, std::tuple<Key, Value>>,
      public detail::base_count<map<Key, Value>, std::tuple<Key, Value>>,
      public detail::base_async_reduce<map<Key, Value>, std::tuple<Key, Value>>,
      public detail::base_async_erase_key<map<Key, Value>,
                                          std::tuple<Key, Value>>,
      public detail::base_async_erase_key_value<map<Key, Value>,
                                                std::tuple<Key, Value>>,
      public detail::base_batch_erase_key_value<map<Key, Value>,
                                                std::tuple<Key, Value>>,
      public detail::base_async_visit<map<Key, Value>, std::tuple<Key, Value>>,
      public detail::base_iterators<map<Key, Value>>,
      public detail::base_iteration_key_value<map<Key, Value>,
                                              std::tuple<Key, Value>> {
  friend struct detail::base_misc<map<Key, Value>, std::tuple<Key, Value>>;

  using local_container_type =
      boost::unordered::unordered_flat_map<Key, Value, detail::hash<Key>>;

 public:
  using self_type      = map<Key, Value>;
  using mapped_type    = Value;
  using ptr_type       = typename ygm::ygm_ptr<self_type>;
  using key_type       = Key;
  using size_type      = size_t;
  using for_all_args   = std::tuple<Key, Value>;
  using container_type = ygm::container::map_tag;
  using iterator       = typename local_container_type::iterator;
  using const_iterator = typename local_container_type::const_iterator;

  map() = delete;

  /**
   * @brief Map constructor
   *
   * @param comm Communicator to use for communication
   */
  map(ygm::comm& comm)
      : m_comm(comm), pthis(this), m_default_value(), partitioner(comm) {
    m_comm.log(log_level::info, "Creating ygm::container::map");
    pthis.check(m_comm);
  }

  /**
   * @brief Map constructor taking default value
   *
   * @param comm Communicator to use for communication
   * @param default_value Value to initialize all stored items with
   */
  map(ygm::comm& comm, const mapped_type& default_value)
      : m_comm(comm),
        pthis(this),
        m_default_value(default_value),
        partitioner(comm) {
    m_comm.log(log_level::info, "Creating ygm::container::map");
    pthis.check(m_comm);
  }

  /**
   * @brief Map constructor from std::initializer_list of key-value pairs
   *
   * @param comm Communicator to use for communication
   * @param l Initializer list of key-value pairs to put in map
   * @details Initializer list is assumed to be replicated on all ranks.
   */
  map(ygm::comm& comm, std::initializer_list<std::pair<Key, Value>> l)
      : m_comm(comm), pthis(this), m_default_value(), partitioner(comm) {
    m_comm.log(log_level::info, "Creating ygm::container::map");
    pthis.check(m_comm);
    if (m_comm.rank0()) {
      for (const std::pair<Key, Value>& i : l) {
        async_insert(i);
      }
    }
  }

  /**
   * @brief Construct map from existing STL container
   *
   * @tparam T Existing container type
   * @param comm Communicator to use for communication
   * @param cont STL container containing key-value pairs to put in map
   */
  template <typename STLContainer>
  map(ygm::comm& comm, const STLContainer& cont)
    requires detail::STLContainer<STLContainer> &&
                 std::convertible_to<typename STLContainer::value_type,
                                     std::pair<Key, Value>>
      : m_comm(comm), pthis(this), m_default_value(), partitioner(comm) {
    m_comm.log(log_level::info, "Creating ygm::container::map");
    pthis.check(m_comm);

    for (const std::pair<Key, Value>& i : cont) {
      this->async_insert(i);
    }
    m_comm.barrier();
  }

  /**
   * @brief Construct map from existing YGM container of key-value pairs
   *
   * @tparam T Existing container type
   * @param comm Communicator to use for communication
   * @param yc YGM container of key-value pairs to put in map.
   * @details Requires input container `for_all_args` to be a single item that
   * is itself a key-value pair.
   */
  template <typename YGMContainer>
  map(ygm::comm& comm, const YGMContainer& yc)
    requires detail::HasForAll<YGMContainer> &&
                 detail::SingleItemTuple<typename YGMContainer::for_all_args>
      : m_comm(comm), pthis(this), m_default_value(), partitioner(comm) {
    m_comm.log(log_level::info, "Creating ygm::container::map");
    pthis.check(m_comm);

    yc.for_all([this](const std::pair<Key, Value>& value) {
      this->async_insert(value);
    });

    m_comm.barrier();
  }

  ~map() {
    m_comm.log(log_level::info, "Destroying ygm::container::map");
    m_comm.barrier();
  }

  map(const self_type& other)
      : m_comm(other.comm()),
        pthis(this),
        m_local_map(other.m_local_map),
        m_default_value(other.m_default_value),
        partitioner(other.comm()) {
    m_comm.log(log_level::info, "Copying ygm::container::map");
    pthis.check(m_comm);
  }

  map(self_type&& other) noexcept
      : m_comm(other.comm()),
        pthis(this),
        m_local_map(std::move(other.m_local_map)),
        m_default_value(other.m_default_value),
        partitioner(other.comm()) {
    m_comm.log(log_level::info, "Moving ygm::container::map");
    pthis.check(m_comm);
  }

  map& operator=(const self_type& other) {
    m_comm.log(log_level::info,
               "Calling ygm::container::map copy assignment operator");
    return *this = map(other);
  }

  map& operator=(self_type&& other) {
    m_comm.log(log_level::info,
               "Calling ygm::container::map move assignment operator");
    std::swap(m_local_map, other.m_local_map);
    std::swap(m_default_value, other.m_default_value);
    return *this;
  }

  /**
   * @brief Access to begin iterator of locally-held items
   *
   * @return Local iterator to beginning of items held by process.
   * @details Does not call `barrier()`.
   */
  iterator local_begin() { return m_local_map.begin(); }

  /**
   * @brief Access to begin const_iterator of locally-held items for const map
   *
   * @return Local const iterator to beginning of items held by process.
   * @details Does not call `barrier()`.
   */
  const_iterator local_begin() const { return m_local_map.cbegin(); }

  /**
   * @brief Access to begin const_iterator of locally-held items for const map
   *
   * @return Local const iterator to beginning of items held by process.
   * @details Does not call `barrier()`.
   */
  const_iterator local_cbegin() const { return m_local_map.cbegin(); }

  /**
   * @brief Access to end iterator of locally-held items
   *
   * @return Local iterator to ending of items held by process.
   * @details Does not call `barrier()`.
   */
  iterator local_end() { return m_local_map.end(); }

  /**
   * @brief Access to end const_iterator of locally-held items for const map
   *
   * @return Local const iterator to ending of items held by process.
   * @details Does not call `barrier()`.
   */
  const_iterator local_end() const { return m_local_map.cend(); }

  /**
   * @brief Access to end const_iterator of locally-held items for const map
   *
   * @return Local const iterator to ending of items held by process.
   * @details Does not call `barrier()`.
   */
  const_iterator local_cend() const { return m_local_map.cend(); }

  using detail::base_async_erase_key<map<Key, Value>,
                                     for_all_args>::async_erase;
  using detail::base_async_erase_key_value<map<Key, Value>,
                                           for_all_args>::async_erase;
  using detail::base_batch_erase_key_value<map<Key, Value>,
                                           for_all_args>::erase;

  /**
   * @brief Insert a key and default value into local storage.
   *
   * @param key Local index to store default value at
   */
  void local_insert(const key_type& key) { local_insert(key, m_default_value); }

  /**
   * @brief Erase local entry for given key
   *
   * @param key Key to erase from local storage
   */
  void local_erase(const key_type& key) { m_local_map.erase(key); }

  /**
   * @brief Erase local entry for given key and value
   *
   * @param key Key to erase from local storage
   * @param value Value to erase if associated to key
   * @details Does not erase the entry if key is found with a different value
   */
  void local_erase(const key_type& key, const key_type& value) {
    auto itr = m_local_map.find(key);
    if (itr != m_local_map.end() && itr->second == value) {
      m_local_map.erase(itr);
    }
  }

  /**
   * @brief Insert a key and value into local storage.
   *
   * @param key Local index to store value at
   * @param value Value to store
   */
  void local_insert(const key_type& key, const mapped_type& value) {
    m_local_map.insert({key, value});
  }

  /**
   * @brief Insert a key and value into local storage or assign value to key if
   * key is already present
   *
   * @param key Local index to store value at
   * @param value Value to store
   */
  void local_insert_or_assign(const key_type& key, const mapped_type& value) {
    m_local_map.insert_or_assign(key, value);
  }

  /**
   * @brief Clear local storage
   */
  void local_clear() { m_local_map.clear(); }

  /**
   * @brief Update a locally stored element by performing a binary operation
   * between it and a provided value
   *
   * @tparam ReductionOp functor type
   * @param key Key to perform binary operation at.
   * @param value Value to combine with the currently-held value
   * @param reducer Binary operation to perform
   */
  template <typename ReductionOp>
  void local_reduce(const key_type& key, const mapped_type& value,
                    ReductionOp reducer) {
    if (m_local_map.count(key) == 0) {
      m_local_map.insert({key, value});
    } else {
      m_local_map[key] = reducer(value, m_local_map[key]);
    }
  }

  /**
   * @brief Get the number of elements stored on the local process.
   *
   * @return Local size of map
   */
  size_t local_size() const { return m_local_map.size(); }

  /**
   * @brief Retrieve value for given key.
   *
   * @param key Key to look up value for
   * @details Throws an exception if key is not found in local storage
   */
  mapped_type& local_at(const key_type& key) { return m_local_map.at(key); }

  /**
   * @brief Retrieve const reference to value for given key.
   *
   * @param key Key to look up value for
   * @details Throws an exception if key is not found in local storage
   */
  const mapped_type& local_at(const key_type& key) const {
    return m_local_map.at(key);
  }

  /**
   * @brief Visit a key-value pair stored locally
   *
   * @tparam Function functor type
   * @tparam VisitorArgs... Variadic argument types
   * @param key Key to visit
   * @param fn User-provided function to execute at item
   * @param args... Arguments to pass to user functor
   */
  template <typename Function, typename... VisitorArgs>
  void local_visit(const key_type& key, Function&& fn,
                   const VisitorArgs&... args) {
    local_insert(key);  // inserts only if missing
    local_visit_if_contains(key, std::forward<Function>(fn), args...);
  }

  /**
   * @brief Visit a key-value pair stored locally if the key is found
   *
   * @tparam Function functor type
   * @tparam VisitorArgs... Variadic argument types
   * @param key Key to visit
   * @param fn User-provided function to execute at item
   * @param args... Arguments to pass to user functor
   * @details Does not create an entry if `key` is not already found in local
   * map
   */
  template <typename Function, typename... VisitorArgs>
  void local_visit_if_contains(const key_type& key, Function&& fn,
                               const VisitorArgs&... args) {
    ygm::detail::interrupt_mask mask(m_comm);
    auto                        range = m_local_map.equal_range(key);
    if constexpr (std::is_invocable<decltype(fn), const key_type&, mapped_type&,
                                    VisitorArgs&...>() ||
                  std::is_invocable<decltype(fn), ptr_type, const key_type&,
                                    mapped_type&, VisitorArgs&...>()) {
      for (auto itr = range.first; itr != range.second; ++itr) {
        ygm::meta::apply_optional(
            std::forward<Function>(fn), std::make_tuple(pthis),
            std::forward_as_tuple(itr->first, itr->second, args...));
      }
    } else {
      static_assert(ygm::detail::always_false<Function>,
                    "remote map lambda signature must be invocable with (const "
                    "&key_type, mapped_type&, ...) or (ptr_type, const "
                    "&key_type, mapped_type&, ...) signatures");
    }
  }

  /**
   * @brief `local_visit_if_contains` for `const` containers
   *
   * @tparam Function functor type
   * @tparam VisitorArgs... Variadic argument types
   * @param key Key to visit
   * @param fn User-provided function to execute at item
   * @param args... Arguments to pass to user functor
   * @details Does not create an entry if `key` is not already found in local
   * map. `fn` is given a `const` key and value for execution.
   */
  template <typename Function, typename... VisitorArgs>
  void local_visit_if_contains(const key_type& key, Function&& fn,
                               const VisitorArgs&... args) const {
    ygm::detail::interrupt_mask mask(m_comm);
    auto                        range = m_local_map.equal_range(key);
    if constexpr (std::is_invocable<decltype(fn), const key_type&, mapped_type&,
                                    VisitorArgs&...>() ||
                  std::is_invocable<decltype(fn), ptr_type, const key_type&,
                                    mapped_type&, VisitorArgs&...>()) {
      for (auto itr = range.first; itr != range.second; ++itr) {
        ygm::meta::apply_optional(
            std::forward<Function>(fn), std::make_tuple(pthis),
            std::forward_as_tuple(itr->first, itr->second, args...));
      }
    } else {
      static_assert(ygm::detail::always_false<Function>,
                    "remote map lambda signature must be invocable with (const "
                    "&key_type, mapped_type&, ...) or (ptr_type, const "
                    "&key_type, mapped_type&, ...) signatures");
    }
  }

  /**
   * @brief Collective operation to look up item counts from each rank
   *
   * @param keys Keys local rank wants to collect counts for
   * @return `std::map` of provided keys and their counts
   */
  template <typename STLKeyContainer>
  std::map<key_type, mapped_type> gather_keys(const STLKeyContainer& keys) {
    std::map<key_type, mapped_type>         to_return;
    static std::map<key_type, mapped_type>& sto_return = to_return;

    auto fetcher = [](auto pcomm, int from, const key_type& key, auto pmap) {
      auto returner = [](const key_type&                 key,
                         const std::vector<mapped_type>& values) {
        for (const auto& v : values) {
          sto_return.insert(std::make_pair(key, v));
        }
      };
      auto values = pmap->local_get(key);
      pcomm->async(from, returner, key, values);
    };

    m_comm.barrier();
    for (const auto& key : keys) {
      int o = partitioner.owner(key);
      m_comm.async(o, fetcher, m_comm.rank(), key, pthis);
    }
    m_comm.barrier();
    return to_return;
  }

  /**
   * @brief Retrieve all values associated to a given key
   *
   * @param key Key to retrieve values for
   * @return Vector of values associated to key
   * @details Currently, `ygm::container::map` is not a multi-map, so there can
   * be at most one value associated to each key.
   */
  std::vector<mapped_type> local_get(const key_type& key) const {
    std::vector<mapped_type> to_return;

    auto range = m_local_map.equal_range(key);
    for (auto itr = range.first; itr != range.second; ++itr) {
      to_return.push_back(itr->second);
    }

    return to_return;
  }

  /**
   * @brief Execute a functor on every locally-held key and value
   *
   * @tparam Function functor type
   * @param fn Functor to execute on keys and values
   */
  template <typename Function>
  void local_for_all(Function&& fn) {
    if constexpr (std::is_invocable<decltype(fn), const key_type,
                                    mapped_type&>()) {
      for (std::pair<const key_type, mapped_type>& kv : m_local_map) {
        fn(kv.first, kv.second);
      }
    } else {
      static_assert(ygm::detail::always_false<Function>,
                    "local map lambda signature must be invocable with (const "
                    "key_type&, mapped_type&) signature");
    }
  }

  /**
   * @brief `local_for_all` for `const` containers
   *
   * @tparam Function functor type
   * @param fn Functor to execute on keys and values
   * @details `const` references to key and value are provided to `fn`
   */
  template <typename Function>
  void local_for_all(Function&& fn) const {
    if constexpr (std::is_invocable<decltype(fn), const key_type,
                                    const mapped_type&>()) {
      for (const std::pair<const key_type, mapped_type>& kv : m_local_map) {
        fn(kv.first, kv.second);
      }
    } else {
      static_assert(ygm::detail::always_false<Function>,
                    "local map lambda signature must be invocable with (const "
                    "key_type&, const mapped_type&) signature");
    }
  }

  // void async_insert(const std::pair<key_type, mapped_type>& kv) {
  //   async_insert(kv.first, kv.second);
  // }

  // template <typename Visitor, typename... VisitorArgs>
  // void async_visit(const key_type& key, Visitor visitor,
  //                  const VisitorArgs&... args) {
  //   m_impl.async_visit(key, visitor, std::forward<const
  //   VisitorArgs>(args)...);
  // }

  // template <typename Visitor, typename... VisitorArgs>
  // void async_visit_if_exists(const key_type& key, Visitor visitor,
  //                            const VisitorArgs&... args) {
  //   m_impl.async_visit_if_exists(key, visitor,
  //                                std::forward<const VisitorArgs>(args)...);
  // }

  // template <typename Visitor, typename... VisitorArgs>
  // void async_insert_if_missing_else_visit(const key_type&    key,
  //                                         const mapped_type& value,
  //                                         Visitor            visitor,
  //                                         const VisitorArgs&... args) {
  //   m_impl.async_insert_if_missing_else_visit(
  //       key, value, visitor, std::forward<const VisitorArgs>(args)...);
  // }

  // template <typename ReductionOp>
  // void async_reduce(const key_type& key, const mapped_type& value,
  //                   ReductionOp reducer) {
  //   m_impl.async_reduce(key, value, reducer);
  // }

  // void async_erase(const key_type& key) { m_impl.async_erase(key); }

  // template <typename Function>
  // void for_all(Function fn) {
  //   m_impl.for_all(fn);
  // }

  /**
   * @brief Count the number of times a given key is found locally
   *
   * @return Number of times `key` is found locally
   */
  size_t local_count(const key_type& key) const {
    return m_local_map.count(key);
  }

  /**
   * @brief Check if a key exists locally
   *
   * @param key key to check for
   * @return True if `key` exists locally, false otherwise
   */
  bool local_contains(const key_type& key) const {
    return m_local_map.contains(key);
  }

  // void serialize(const std::string& fname) { m_impl.serialize(fname); }
  // void deserialize(const std::string& fname) { m_impl.deserialize(fname); }

  // template <typename STLKeyContainer>
  // std::map<key_type, mapped_type> all_gather(const STLKeyContainer& keys) {
  //   std::map<key_type, mapped_type> to_return;
  //   m_impl.all_gather(keys, to_return);
  //   return to_return;
  // }

  // std::map<key_type, mapped_type> all_gather(
  //     const std::vector<key_type>& keys) {
  //   std::map<key_type, mapped_type> to_return;
  //   m_impl.all_gather(keys, to_return);
  //   return to_return;
  // }

  // template <typename CompareFunction>
  // std::vector<std::pair<key_type, mapped_type>> topk(size_t          k,
  //                                                    CompareFunction cfn) {
  //   return m_impl.topk(k, cfn);
  // }

  /**
   * @brief Swap the local contents of a map.
   *
   * @param other The map to swap local contents with
   */
  void local_swap(self_type& other) { m_local_map.swap(other.m_local_map); }

 private:
  ygm::comm&                       m_comm;
  typename ygm::ygm_ptr<self_type> pthis;
  local_container_type             m_local_map;
  mapped_type                      m_default_value;

 public:
  detail::hash_partitioner<detail::hash<key_type>> partitioner;
};
}  // namespace ygm::container
