// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once
#include <fstream>
#include <unordered_map>
#include <vector>
#include <ygm/comm.hpp>
#include <ygm/container/container_traits.hpp>
#include <ygm/container/detail/hash_partitioner.hpp>
#include <ygm/detail/ygm_ptr.hpp>
#include <ygm/detail/ygm_traits.hpp>

namespace ygm::container::detail {
template <typename Item, typename Partitioner>
class disjoint_set_impl {
 public:
  struct data_t;

  using self_type         = disjoint_set_impl<Item, Partitioner>;
  using self_ygm_ptr_type = typename ygm::ygm_ptr<self_type>;
  using value_type        = Item;
  using size_type         = size_t;
  using ygm_for_all_types = std::tuple<Item, Item>;
  using container_type    = ygm::container::disjoint_set_tag;
  using rank_type         = int16_t;
  using item_map_type     = std::map<value_type, data_t>;

  Partitioner partitioner;

  struct data_t {
   public:
    friend disjoint_set_impl;

    const value_type &get_parent() const { return m_parent; }

    rank_type get_rank() const { return m_rank; }

    rank_type get_parent_rank_estimate() const { return m_parent_rank_est; }

    template <typename Archive>
    void serialize(Archive &ar) {
      ar(m_parent, m_rank, m_parent_rank_est);
    }

   private:
    void increase_rank(const rank_type new_rank) {
      YGM_ASSERT_RELEASE(m_rank < new_rank);
      m_rank = new_rank;

      // Only called on roots
      m_parent_rank_est = new_rank;
    }

    // Set parent if estimated parent rank is no worse than current estimate
    void set_parent(const value_type &parent, const rank_type parent_rank_est) {
      if (parent_rank_est >= m_parent_rank_est) {
        m_parent          = parent;
        m_parent_rank_est = parent_rank_est;
      }
    }

    void set_parent(const value_type &parent) { m_parent = parent; }

    value_type m_parent;
    rank_type  m_rank            = 0;
    rank_type  m_parent_rank_est = 0;
  };

  class hash_cache {
   public:
    class cache_entry {
     public:
      cache_entry() : occupied(false) {}

      cache_entry(const value_type &_item, const value_type &_parent,
                  const rank_type _parent_rank_est)
          : item(_item), parent(_parent), parent_rank_est(_parent_rank_est) {}

      bool       occupied = false;
      value_type item;
      value_type parent;
      rank_type  parent_rank_est;
    };

    hash_cache(const size_t cache_size)
        : m_cache_size(cache_size), m_cache(cache_size) {
      for (size_t i = 0; i < m_cache_size; ++i) {
        m_cache[i] = cache_entry();
      }
    }

    void add_cache_entry(const value_type &item, const value_type &parent,
                         const rank_type parent_rank_est) {
      size_t index = std::hash<value_type>()(item) % m_cache_size;

      auto &current_entry = m_cache[index];

      // Only replace cached value if current slot is empty or if new entry's
      // rank is higher
      if (current_entry.occupied == false ||
          parent_rank_est >= current_entry.parent_rank_est) {
        current_entry.occupied        = true;
        current_entry.item            = item;
        current_entry.parent          = parent;
        current_entry.parent_rank_est = parent_rank_est;
      }
    }

    const cache_entry &get_cache_entry(const value_type &item) {
      size_t index = std::hash<value_type>()(item) % m_cache_size;

      return m_cache[index];
    }

    void clear() {
      for (auto &entry : m_cache) {
        entry.occupied = false;
      }
    }

    // private:
    size_t                   m_cache_size;
    std::vector<cache_entry> m_cache;
  };

  disjoint_set_impl(ygm::comm &comm, const size_t cache_size)
      : m_comm(comm), pthis(this), m_cache(cache_size) {
    m_comm.log(log_level::info, "Creating ygm::container::disjoint_set");
    pthis.check(m_comm);
  }

  ~disjoint_set_impl() {
    m_comm.log(log_level::info, "Destroying ygm::container::disjoint_set");
    m_comm.barrier();
  }

  typename ygm::ygm_ptr<self_type> get_ygm_ptr() const { return pthis; }

  template <typename Visitor, typename... VisitorArgs>
  void async_visit(const value_type &item, Visitor visitor,
                   const VisitorArgs &...args) {
    int  dest          = owner(item);
    auto visit_wrapper = [visitor](auto p_dset, const value_type &item,
                                   const VisitorArgs &...args) mutable {
      auto item_data_iter = p_dset->m_local_item_map.find(item);
      if (item_data_iter == p_dset->m_local_item_map.end()) {
        data_t new_item_data;
        new_item_data.set_parent(item, 0);
        item_data_iter =
            p_dset->m_local_item_map.insert(std::make_pair(item, new_item_data))
                .first;
      }

      ygm::meta::apply_optional(
          visitor, std::make_tuple(p_dset),
          std::forward_as_tuple(*item_data_iter, args...));
    };

    m_comm.async(dest, visit_wrapper, pthis, item,
                 std::forward<const VisitorArgs>(args)...);
  }

  void async_union(const value_type &a, const value_type &b) {
    m_is_compressed = false;
    static auto update_parent_and_cache_lambda =
        [](auto p_dset, auto &item_data, const value_type &old_parent,
           const value_type &new_parent, const rank_type &new_parent_rank_est) {
          p_dset->m_cache.add_cache_entry(old_parent, new_parent,
                                          new_parent_rank_est);

          if (old_parent == item_data.second.get_parent()) {
            item_data.second.set_parent(new_parent, new_parent_rank_est);
          }
        };

    static auto resolve_merge_lambda = [](auto p_dset, auto &item_data,
                                          const value_type &merging_item,
                                          const rank_type   merging_rank) {
      const auto &my_item   = item_data.first;
      const auto  my_rank   = item_data.second.get_rank();
      const auto &my_parent = item_data.second.get_parent();
      const auto  my_parent_rank_est =
          item_data.second.get_parent_rank_estimate();
      YGM_ASSERT_RELEASE(my_rank >= merging_rank);

      if (my_rank > merging_rank) {
        return;
      } else {
        YGM_ASSERT_RELEASE(my_rank == merging_rank);
        if (my_parent ==
            my_item) {  // Merging new item onto root. Need to increase rank.
          item_data.second.increase_rank(merging_rank + 1);
        } else {  // Tell merging item about new parent
          p_dset->async_visit(
              merging_item,
              [](auto &item_data, const value_type &new_parent,
                 const rank_type &new_parent_rank_est) {
                item_data.second.set_parent(new_parent, new_parent_rank_est);
              },
              my_parent, my_parent_rank_est);
        }
      }
    };

    // Walking up parent trees can be expressed as a recursive operation
    struct simul_parent_walk_functor {
      void operator()(self_ygm_ptr_type                    p_dset,
                      std::pair<const value_type, data_t> &my_item_data,
                      const value_type &my_child, value_type other_parent,
                      value_type other_item, rank_type other_rank) {
        // Note: other_item needs rank info for comparison with my_item's
        // parent. All others need rank and item to determine if other_item
        // has been visited/initialized.

        value_type my_item   = my_item_data.first;
        rank_type  my_rank   = my_item_data.second.get_rank();
        value_type my_parent = my_item_data.second.get_parent();
        rank_type  my_parent_rank_est =
            my_item_data.second.get_parent_rank_estimate();

        std::tie(my_parent, my_parent_rank_est) =
            p_dset->walk_cache(my_parent, my_rank);
        std::tie(other_parent, other_rank) =
            p_dset->walk_cache(other_parent, other_rank);

        // Path splitting
        if (my_child != my_item) {
          p_dset->async_visit(my_child, update_parent_and_cache_lambda, my_item,
                              my_parent, my_parent_rank_est);
        }

        if (my_parent == other_parent || my_parent == other_item) {
          return;
        }

        if (my_parent_rank_est > other_rank) {  // Other path has lower rank
          p_dset->async_visit(other_parent, simul_parent_walk_functor(),
                              other_item, my_parent, my_item,
                              my_parent_rank_est);
        }

        if (my_parent_rank_est < other_rank) {  // Current path has lower rank
          if (my_parent == my_item) {           // At a root
            my_item_data.second.set_parent(
                other_parent, other_rank);  // Safe to attach to other path

            return;
          } else {  // Not at a root
            // Continue walking current path
            p_dset->async_visit(my_parent, simul_parent_walk_functor(), my_item,
                                other_parent, other_item, other_rank);
          }
        }

        if (my_parent_rank_est == other_rank) {
          if (my_parent == my_item) {  // At a root

            if (my_item < other_parent) {  // Need to break ties in rank before
                                           // merging to avoid cycles of merges
                                           // creating cycles in disjoint set
              // Perform merge
              my_item_data.second.set_parent(
                  other_parent,
                  my_rank);  // other_parent may be of same rank as my_item

              p_dset->async_visit(other_parent, resolve_merge_lambda, my_item,
                                  my_rank);
            } else {
              // Switch to other path to attempt merge
              p_dset->async_visit(other_parent, simul_parent_walk_functor(),
                                  other_item, my_parent, my_item, my_rank);
            }
          } else {  // Not at a root
            // Continue walking current path
            p_dset->async_visit(my_parent, simul_parent_walk_functor(), my_item,
                                other_parent, other_item, other_rank);
          }
        }
      }
    };

    // Walk cache for initial items
    value_type my_item   = a;
    rank_type  my_rank   = -1;
    value_type my_parent = a;

    value_type other_item   = b;
    rank_type  other_rank   = -1;
    value_type other_parent = b;

    std::tie(my_parent, my_rank)       = walk_cache(my_parent, my_rank);
    std::tie(other_parent, other_rank) = walk_cache(other_parent, other_rank);

    if (my_rank <= other_rank) {
      async_visit(my_parent, simul_parent_walk_functor(), my_item, other_parent,
                  other_item, other_rank);
    } else {
      async_visit(other_parent, simul_parent_walk_functor(), other_item,
                  my_parent, my_item, my_rank);
    }
  }

  template <typename Function, typename... FunctionArgs>
  void async_union_and_execute(const value_type &a, const value_type &b,
                               Function fn, const FunctionArgs &...args) {
    m_is_compressed = false;
    static auto update_parent_and_cache_lambda =
        [](auto p_dset, auto &item_data, const value_type &old_parent,
           const value_type &new_parent, const rank_type &new_parent_rank_est) {
          p_dset->m_cache.add_cache_entry(old_parent, new_parent,
                                          new_parent_rank_est);

          if (old_parent == item_data.second.get_parent()) {
            item_data.second.set_parent(new_parent, new_parent_rank_est);
          }
        };

    static auto resolve_merge_lambda = [](auto p_dset, auto &item_data,
                                          const value_type &merging_item,
                                          const rank_type   merging_rank) {
      const auto &my_item   = item_data.first;
      const auto  my_rank   = item_data.second.get_rank();
      const auto &my_parent = item_data.second.get_parent();
      const auto  my_parent_rank_est =
          item_data.second.get_parent_rank_estimate();
      YGM_ASSERT_RELEASE(my_rank >= merging_rank);

      if (my_rank > merging_rank) {
        return;
      } else {
        YGM_ASSERT_RELEASE(my_rank == merging_rank);
        if (my_parent == my_item) {  // Has not found new parent
          item_data.second.increase_rank(merging_rank + 1);
        } else {  // Tell merging item about new parent
          p_dset->async_visit(
              merging_item,
              [](auto &item_data, const value_type &new_parent,
                 const rank_type new_parent_rank_est) {
                item_data.second.set_parent(new_parent, new_parent_rank_est);
              },
              my_parent, my_parent_rank_est);
        }
      }
    };

    // Walking up parent trees can be expressed as a recursive operation
    struct simul_parent_walk_functor {
      void operator()(self_ygm_ptr_type                    p_dset,
                      std::pair<const value_type, data_t> &my_item_data,
                      const value_type &my_child, value_type other_parent,
                      value_type other_item, rank_type other_rank,
                      const value_type &orig_a, const value_type &orig_b,
                      const FunctionArgs &...args) {
        // Note: other_item needs rank info for comparison with my_item's
        // parent. All others need rank and item to determine if other_item
        // has been visited/initialized.

        value_type my_item   = my_item_data.first;
        rank_type  my_rank   = my_item_data.second.get_rank();
        value_type my_parent = my_item_data.second.get_parent();
        rank_type  my_parent_rank_est =
            my_item_data.second.get_parent_rank_estimate();

        my_parent_rank_est = std::max<rank_type>(my_rank, my_parent_rank_est);

        std::tie(my_parent, my_parent_rank_est) =
            p_dset->walk_cache(my_parent, my_rank);
        std::tie(other_parent, other_rank) =
            p_dset->walk_cache(other_parent, other_rank);

        // Path splitting
        if (my_child != my_item) {
          p_dset->async_visit(my_child, update_parent_and_cache_lambda, my_item,
                              my_parent, my_rank);
        }

        if (my_parent == other_parent || my_parent == other_item) {
          // Perform user function for unsuccessful merge
          Function *f = nullptr;
          ygm::meta::apply_optional(
              *f, std::make_tuple(p_dset),
              std::forward_as_tuple(orig_a, orig_b, false, args...));
          return;
        }

        if (my_parent_rank_est > other_rank) {  // Other path has lower rank
          p_dset->async_visit(other_parent, simul_parent_walk_functor(),
                              other_item, my_parent, my_item,
                              my_parent_rank_est, orig_a, orig_b, args...);
        }

        if (my_parent_rank_est < other_rank) {  // Current path has lower rank
          if (my_parent == my_item) {           // At a root
            my_item_data.second.set_parent(
                other_parent, other_rank);  // Safe to attach to other path

            // Perform user function after successful merge
            Function *f = nullptr;
            if constexpr (std::is_invocable<decltype(fn), const value_type &,
                                            const value_type &, const bool,
                                            FunctionArgs &...>() ||
                          std::is_invocable<decltype(fn), self_ygm_ptr_type,
                                            const value_type &,
                                            const value_type &, const bool,
                                            FunctionArgs &...>()) {
              ygm::meta::apply_optional(
                  *f, std::make_tuple(p_dset),
                  std::forward_as_tuple(orig_a, orig_b, true, args...));
            } else {
              static_assert(
                  ygm::detail::always_false<Function>,
                  "remote disjoint_set lambda signature must be invocable "
                  "with (const value_type &, const value_type &, const bool) "
                  "signature");
            }

            return;

          } else {  // Not at a root
            //   Continue walking current path
            p_dset->async_visit(my_parent, simul_parent_walk_functor(), my_item,
                                other_parent, other_item, other_rank, orig_a,
                                orig_b, args...);
          }
        }

        if (my_parent_rank_est == other_rank) {
          if (my_parent == my_item) {      // At a root
            if (my_item < other_parent) {  // Need to break ties in rank before
              // merging to avoid cycles of merges
              // creating cycles in disjoint set
              // Perform merge

              // Guaranteed any path through current
              // item will find an item with rank >=
              // my_rank+1 by going to other_parent
              // Cannot set parent_rank_est to my_rank+1 until certain new
              // parent's rank has been updated
              my_item_data.second.set_parent(other_parent, my_rank);

              // Perform user function after successful merge
              Function *f = nullptr;
              if constexpr (std::is_invocable<decltype(fn), const value_type &,
                                              const value_type &, const bool,
                                              FunctionArgs &...>() ||
                            std::is_invocable<decltype(fn), self_ygm_ptr_type,
                                              const value_type &,
                                              const value_type &, const bool,
                                              FunctionArgs &...>()) {
                ygm::meta::apply_optional(
                    *f, std::make_tuple(p_dset),
                    std::forward_as_tuple(orig_a, orig_b, true, args...));
              } else {
                static_assert(
                    ygm::detail::always_false<Function>,
                    "remote disjoint_set lambda signature must be invocable "
                    "with (const value_type &, const value_type &, const bool) "
                    "signature");
              }

              p_dset->async_visit(other_parent, resolve_merge_lambda, my_item,
                                  my_rank);
            } else {
              //   Switch to other path to attempt merge
              p_dset->async_visit(other_parent, simul_parent_walk_functor(),
                                  other_item, my_parent, my_item, my_rank,
                                  orig_a, orig_b, args...);
            }
          } else {  // Not at a root
            //   Continue walking current path
            p_dset->async_visit(my_parent, simul_parent_walk_functor(), my_item,
                                other_parent, other_item, other_rank, orig_a,
                                orig_b, args...);
          }
        }
      }
    };

    // Walk cache for initial items
    value_type my_item   = a;
    rank_type  my_rank   = -1;
    value_type my_parent = a;

    value_type other_item   = b;
    rank_type  other_rank   = -1;
    value_type other_parent = b;

    std::tie(my_parent, my_rank)       = walk_cache(my_parent, my_rank);
    std::tie(other_parent, other_rank) = walk_cache(other_parent, other_rank);

    if (my_rank <= other_rank) {
      async_visit(my_parent, simul_parent_walk_functor(), my_item, other_parent,
                  other_item, other_rank, a, b, args...);
    } else {
      async_visit(other_parent, simul_parent_walk_functor(), other_item,
                  my_parent, my_item, my_rank, a, b, args...);
    }
  }

  void all_compress() {
    m_comm.barrier();

    // Exit if no async_union(_and_execute) since last compress
    if (logical_and(m_is_compressed, m_comm) == true) {
      return;
    }

    struct rep_query {
      value_type              rep;
      std::vector<value_type> local_inquiring_items;
    };

    struct item_status {
      bool             found_root;
      std::vector<int> held_responses;
    };

    static std::unordered_map<value_type, rep_query> queries;
    static std::unordered_map<value_type, item_status>
        local_item_status;  // For holding incoming queries while my items are
                            // waiting for their representatives (only needed
                            // for when parent rank is same as mine)
    queries.clear();
    local_item_status.clear();

    struct update_rep_functor {
     public:
      void operator()(self_ygm_ptr_type p_dset, const value_type &parent,
                      const value_type &rep) {
        auto &local_rep_query = queries.at(parent);
        local_rep_query.rep   = rep;

        for (const auto &local_item : local_rep_query.local_inquiring_items) {
          p_dset->m_local_item_map[local_item].set_parent(rep);

          // Forward rep for any held responses
          auto local_item_statuses_iter = local_item_status.find(local_item);
          if (local_item_statuses_iter != local_item_status.end()) {
            for (int dest : local_item_statuses_iter->second.held_responses) {
              p_dset->comm().async(dest, update_rep_functor(), p_dset,
                                   local_item, rep);
            }
            local_item_statuses_iter->second.found_root = true;
            local_item_statuses_iter->second.held_responses.clear();
          }
        }
        local_rep_query.local_inquiring_items.clear();
      }
    };

    auto query_rep_lambda = [](self_ygm_ptr_type p_dset, const value_type &item,
                               int inquiring_rank) {
      const auto &item_info = p_dset->m_local_item_map[item];

      // May need to hold because this item is in the current level
      auto local_item_status_iter = local_item_status.find(item);
      // If query is ongoing for my parent, hold response
      if ((local_item_status_iter != local_item_status.end()) &&
          (local_item_status_iter->second.found_root == false)) {
        local_item_status[item].held_responses.push_back(inquiring_rank);
      } else {
        p_dset->comm().async(inquiring_rank, update_rep_functor(), p_dset, item,
                             item_info.get_parent());
      }
    };

    m_comm.barrier();

    // Prepare all queries and local_item_status objects for rank
    for (const auto &[local_item, item_info] : m_local_item_map) {
      if (item_info.get_parent() != local_item) {
        local_item_status[local_item].found_root = false;

        auto query_iter = queries.find(item_info.get_parent());
        if (query_iter == queries.end()) {  // Have not queried for parent's
                                            // rep. Begin new query.
          auto &new_query = queries[item_info.get_parent()];
          new_query.rep   = item_info.get_parent();
          new_query.local_inquiring_items.push_back(local_item);

        } else {
          query_iter->second.local_inquiring_items.push_back(local_item);
        }
      }
    }

    m_comm.cf_barrier();

    // Start all queries
    for (const auto &[item, query] : queries) {
      int dest = owner(item);
      m_comm.async(dest, query_rep_lambda, pthis, item, m_comm.rank());
    }

    m_comm.barrier();

    m_is_compressed = true;
  }

  template <typename Function>
  void for_all(Function fn) {
    all_compress();

    if constexpr (std::is_invocable<decltype(fn), const value_type &,
                                    const value_type &>()) {
      const auto end = m_local_item_map.end();
      for (auto iter = m_local_item_map.begin(); iter != end; ++iter) {
        const auto &[item, item_data] = *iter;
        fn(item, item_data.get_parent());
      }
    } else {
      static_assert(ygm::detail::always_false<Function>,
                    "local disjoint_set lambda signature must be invocable "
                    "with (const value_type &, const value_type &) signature");
    }
  }

  std::map<value_type, value_type> all_find(
      const std::vector<value_type> &items) {
    m_comm.barrier();

    using return_type = std::map<value_type, value_type>;
    return_type          to_return;
    ygm_ptr<return_type> p_to_return(&to_return);

    struct find_rep_functor {
      void operator()(self_ygm_ptr_type pdset, ygm_ptr<return_type> p_to_return,
                      const value_type &source_item, const int source_rank,
                      const value_type &local_item) {
        const auto parent = pdset->m_local_item_map[local_item].get_parent();

        // Found root
        if (parent == local_item) {
          // Send message to update parent of original item
          int dest = pdset->owner(source_item);
          pdset->comm().async(
              dest,
              [](self_ygm_ptr_type pdset, const value_type &source_item,
                 const value_type &root) {
                pdset->m_local_item_map[source_item].set_parent(root);
              },
              pdset, source_item, parent);
          // Send message to store return value
          pdset->comm().async(
              source_rank,
              [](ygm_ptr<return_type> p_to_return,
                 const value_type    &source_item,
                 const value_type &rep) { (*p_to_return)[source_item] = rep; },
              p_to_return, source_item, parent);
        } else {
          int dest = pdset->owner(parent);
          pdset->comm().async(dest, find_rep_functor(), pdset, p_to_return,
                              source_item, source_rank, parent);
        }
      }
    };

    for (size_type i = 0; i < items.size(); ++i) {
      int dest = owner(items[i]);
      m_comm.async(dest, find_rep_functor(), pthis, p_to_return, items[i],
                   m_comm.rank(), items[i]);
    }

    m_comm.barrier();
    return to_return;
  }

  void clear() {
    m_comm.barrier();
    m_local_item_map.clear();
    m_cache.clear();
  }

  size_t size() {
    m_comm.barrier();
    return ::ygm::sum(m_local_item_map.size(), m_comm);
  }

  size_type num_sets() {
    m_comm.barrier();
    size_t num_local_sets{0};
    for (const auto &[item, item_data] : m_local_item_map) {
      if (item == item_data.get_parent()) {
        ++num_local_sets;
      }
    }
    return ::ygm::sum(num_local_sets, m_comm);
  }

  int owner(const value_type &item) const {
    auto [owner, rank] = partitioner(item, m_comm.size(), 1024);
    return owner;
  }

  bool is_mine(const value_type &item) const {
    return owner(item) == m_comm.rank();
  }

  rank_type max_rank() {
    rank_type local_max{0};

    for (const auto &[item, item_data] : m_local_item_map) {
      local_max = std::max<rank_type>(local_max, item_data.get_rank());
    }

    return max(local_max, m_comm);
  }

  ygm::comm &comm() { return m_comm; }

 private:
  const std::pair<value_type, rank_type> walk_cache(const value_type &item,
                                                    const rank_type  &r) {
    const typename hash_cache::cache_entry *prev_cache_entry = nullptr;
    const typename hash_cache::cache_entry *curr_cache_entry =
        &m_cache.get_cache_entry(item);

    // Don't walk cache if first item is wrong
    if (curr_cache_entry->item != item || not curr_cache_entry->occupied ||
        curr_cache_entry->parent_rank_est < r) {
      return std::make_pair(item, r);
    }

    do {
      prev_cache_entry = curr_cache_entry;
      curr_cache_entry = &m_cache.get_cache_entry(prev_cache_entry->parent);
    } while (prev_cache_entry->parent == curr_cache_entry->item &&
             curr_cache_entry->occupied &&
             prev_cache_entry->item != curr_cache_entry->item &&
             prev_cache_entry->parent_rank_est <=
                 curr_cache_entry->parent_rank_est);

    return std::make_pair(prev_cache_entry->parent,
                          prev_cache_entry->parent_rank_est);
  }

 protected:
  disjoint_set_impl() = delete;

  ygm::comm        &m_comm;
  self_ygm_ptr_type pthis;
  item_map_type     m_local_item_map;

  hash_cache m_cache;

  bool m_is_compressed = true;
};
}  // namespace ygm::container::detail
