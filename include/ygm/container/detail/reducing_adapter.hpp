// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once
#include <ygm/container/container_traits.hpp>
#include <ygm/detail/ygm_ptr.hpp>
#include <ygm/detail/ygm_traits.hpp>

namespace ygm::container::detail {

template <typename Container, typename ReductionOp>
class reducing_adapter {
 public:
  using self_type   = reducing_adapter<Container, ReductionOp>;
  using mapped_type = typename Container::mapped_type;
  using key_type    = typename Container::key_type;
  // using value_type        = typename Container::value_type;

  const size_t cache_size = 1024 * 1024;

  reducing_adapter(Container &c, ReductionOp reducer)
      : m_container(c), m_reducer(reducer), pthis(this) {
    pthis.check(c.comm());
    m_cache.resize(cache_size);
  }

  ~reducing_adapter() { m_container.comm().barrier(); }

  void async_reduce(const key_type &key, const mapped_type &value) {
    cache_reduce(key, value);
  }

 private:
  struct cache_entry {
    key_type    key;
    mapped_type value;
    bool        occupied = false;
  };

  void cache_reduce(const key_type &key, const mapped_type &value) {
    // Bypass cache if current rank owns key
    if (m_container.comm().rank() == m_container.partitioner.owner(key)) {
      container_reduction(key, value);
    } else {
      if (m_cache_empty) {
        m_cache_empty = false;
        m_container.comm().register_pre_barrier_callback(
            [this]() { this->cache_flush_all(); });
      }

      size_t slot = std::hash<key_type>{}(key) % cache_size;

      if (m_cache[slot].occupied == false) {
        m_cache[slot].key      = key;
        m_cache[slot].value    = value;
        m_cache[slot].occupied = true;
      } else {  // Slot is occupied
        if (m_cache[slot].key == key) {
          m_cache[slot].value = m_reducer(m_cache[slot].value, value);
        } else {
          cache_flush(slot);
          YGM_ASSERT_DEBUG(m_cache[slot].occupied == false);
          m_cache[slot].key      = key;
          m_cache[slot].value    = value;
          m_cache[slot].occupied = true;
        }
      }
    }
  }

  void cache_flush(const size_t slot) {
    // Use NLNR for reductions
    int next_dest = m_container.comm().router().next_hop(
        m_container.partitioner.owner(m_cache[slot].key),
        ygm::detail::routing_type::NLNR);

    m_container.comm().async(
        next_dest,
        [](auto p_reducing_adapter, const key_type &key,
           const mapped_type &value) {
          p_reducing_adapter->cache_reduce(key, value);
        },
        pthis, m_cache[slot].key, m_cache[slot].value);

    m_cache[slot].occupied = false;
  }

  void cache_flush_all() {
    for (size_t i = 0; i < cache_size; ++i) {
      if (m_cache[i].occupied) {
        cache_flush(i);
      }
    }
    m_cache_empty = true;
  }

  void container_reduction(const key_type &key, const mapped_type &value) {
    if constexpr (ygm::container::check_container_type<
                      Container, ygm::container::map_tag>()) {
      m_container.async_reduce(key, value, m_reducer);

    } else if constexpr (ygm::container::check_container_type<
                             Container, ygm::container::array_tag>()) {
      m_container.async_binary_op_update_value(key, value, m_reducer);
    } else {
      static_assert(ygm::detail::always_false<ReductionOp>,
                    "Container unsuitable for reducing_adapter");
    }
  }

  std::vector<cache_entry> m_cache;
  bool                     m_cache_empty = true;

  Container                       &m_container;
  ReductionOp                      m_reducer;
  typename ygm::ygm_ptr<self_type> pthis;
};

template <typename Container, typename ReductionOp>
reducing_adapter<Container, ReductionOp> make_reducing_adapter(
    Container &c, ReductionOp reducer) {
  return reducing_adapter<Container, ReductionOp>(c, reducer);
}

}  // namespace ygm::container::detail
