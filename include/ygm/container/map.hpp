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
  friend class detail::base_misc<map<Key, Value>, std::tuple<Key, Value>>;

  using local_container_type =
      boost::unordered::unordered_flat_map<Key, Value, std::hash<Key>>;

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

  map(ygm::comm& comm)
      : m_comm(comm), pthis(this), partitioner(comm), m_default_value() {
    m_comm.log(log_level::info, "Creating ygm::container::map");
    pthis.check(m_comm);
  }

  map(ygm::comm& comm, const mapped_type& default_value)
      : m_comm(comm),
        pthis(this),
        partitioner(comm),
        m_default_value(default_value) {
    m_comm.log(log_level::info, "Creating ygm::container::map");
    pthis.check(m_comm);
  }

  map(ygm::comm& comm, std::initializer_list<std::pair<Key, Value>> l)
      : m_comm(comm), pthis(this), partitioner(comm), m_default_value() {
    m_comm.log(log_level::info, "Creating ygm::container::map");
    pthis.check(m_comm);
    if (m_comm.rank0()) {
      for (const std::pair<Key, Value>& i : l) {
        async_insert(i);
      }
    }
  }

  template <typename STLContainer>
  map(ygm::comm& comm, const STLContainer& cont)
    requires detail::STLContainer<STLContainer> &&
                 std::convertible_to<typename STLContainer::value_type,
                                     std::pair<Key, Value>>
      : m_comm(comm), pthis(this), partitioner(comm), m_default_value() {
    m_comm.log(log_level::info, "Creating ygm::container::map");
    pthis.check(m_comm);

    for (const std::pair<Key, Value>& i : cont) {
      this->async_insert(i);
    }
    m_comm.barrier();
  }

  template <typename YGMContainer>
  map(ygm::comm& comm, const YGMContainer& yc)
    requires detail::HasForAll<YGMContainer> &&
                 detail::SingleItemTuple<typename YGMContainer::for_all_args>
      : m_comm(comm), pthis(this), partitioner(comm), m_default_value() {
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
        partitioner(other.comm()),
        m_default_value(other.m_default_value),
        m_local_map(other.m_local_map) {
    m_comm.log(log_level::info, "Copying ygm::container::map");
    pthis.check(m_comm);
  }

  map(self_type&& other) noexcept
      : m_comm(other.comm()),
        pthis(this),
        partitioner(other.comm()),
        m_default_value(other.m_default_value),
        m_local_map(std::move(other.m_local_map)) {
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

  iterator       local_begin() { return m_local_map.begin(); }
  const_iterator local_begin() const { return m_local_map.cbegin(); }
  const_iterator local_cbegin() const { return m_local_map.cbegin(); }

  iterator       local_end() { return m_local_map.end(); }
  const_iterator local_end() const { return m_local_map.cend(); }
  const_iterator local_cend() const { return m_local_map.cend(); }

  using detail::base_async_erase_key<map<Key, Value>,
                                     for_all_args>::async_erase;
  using detail::base_async_erase_key_value<map<Key, Value>,
                                           for_all_args>::async_erase;
  using detail::base_batch_erase_key_value<map<Key, Value>,
                                           for_all_args>::erase;

  void local_insert(const key_type& key) { local_insert(key, m_default_value); }

  void local_erase(const key_type& key) { m_local_map.erase(key); }

  void local_erase(const key_type& key, const key_type& value) {
    auto itr = m_local_map.find(key);
    if (itr != m_local_map.end() && itr->second == value) {
      m_local_map.erase(itr);
    }
  }

  void local_insert(const key_type& key, const mapped_type& value) {
    m_local_map.insert({key, value});
  }

  void local_insert_or_assign(const key_type& key, const mapped_type& value) {
    m_local_map.insert_or_assign(key, value);
  }

  void local_clear() { m_local_map.clear(); }

  template <typename ReductionOp>
  void local_reduce(const key_type& key, const mapped_type& value,
                    ReductionOp reducer) {
    if (m_local_map.count(key) == 0) {
      m_local_map.insert({key, value});
    } else {
      m_local_map[key] = reducer(value, m_local_map[key]);
    }
  }

  size_t local_size() const { return m_local_map.size(); }

  mapped_type& local_at(const key_type& key) { return m_local_map.at(key); }

  const mapped_type& local_at(const key_type& key) const {
    return m_local_map.at(key);
  }

  template <typename Function, typename... VisitorArgs>
  void local_visit(const key_type& key, Function&& fn,
                   const VisitorArgs&... args) {
    local_insert(key);  // inserts only if missing
    local_visit_if_contains(key, std::forward<Function>(fn), args...);
  }

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

  template <typename STLKeyContainer>
  std::map<key_type, mapped_type> gather_keys(const STLKeyContainer& keys) {
    std::map<key_type, mapped_type>         to_return;
    static std::map<key_type, mapped_type>& sto_return = to_return;

    auto fetcher = [](auto pcomm, int from, const key_type& key, auto pmap) {
      auto returner = [](auto pcomm, const key_type& key,
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

  std::vector<mapped_type> local_get(const key_type& key) const {
    std::vector<mapped_type> to_return;

    auto range = m_local_map.equal_range(key);
    for (auto itr = range.first; itr != range.second; ++itr) {
      to_return.push_back(itr->second);
    }

    return to_return;
  }

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

  size_t local_count(const key_type& key) const {
    return m_local_map.count(key);
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

  detail::hash_partitioner<std::hash<key_type>> partitioner;

  void local_swap(self_type& other) { m_local_map.swap(other.m_local_map); }

 private:
  ygm::comm&                       m_comm;
  local_container_type             m_local_map;
  mapped_type                      m_default_value;
  typename ygm::ygm_ptr<self_type> pthis;
};
}  // namespace ygm::container
