
// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <ygm/comm.hpp>
#include <ygm/detail/collective.hpp> 
#include <ygm/container/detail/base_concepts.hpp> 
#include <cmath>

namespace ygm::random {

template<typename Item, typename T>
concept pair_like_and_convertible_to_weighted_item = 
    (requires { typename T::first_type; typename T::second_type; } &&
      std::is_convertible_v<T, std::pair<Item, double>>) ||
    (std::tuple_size_v<T> == 2 &&
      std::is_convertible_v<std::tuple_element_t<0,T>, Item> &&
      std::is_convertible_v<std::tuple_element_t<1,T>, double>);

template <typename Item>
class alias_table {

 public: 
  using self_type = alias_table<Item>;

  struct item {
      Item id;
      double weight;

      template <typename Archive>
      void serialize(Archive& ar) {
          ar(id, weight);
      }
  };

  struct table_item { 
      // p / m_avg_weight = prob item a is selected. 1 - p / m_avg_weight is prob b is selected.
      double p; 
      Item a;
      Item b;
  };

  template <typename YGMContainer>
  requires ygm::container::detail::HasForAll<YGMContainer> &&
           ygm::container::detail::SingleItemTuple<typename YGMContainer::for_all_args> &&
           pair_like_and_convertible_to_weighted_item<Item,
            std::tuple_element_t<0,typename YGMContainer::for_all_args>>
  alias_table(ygm::comm &comm, YGMContainer &c,
    std::optional<std::uint32_t> seed = std::nullopt) 
    : m_comm(comm), pthis(this), m_rank_dist(0, comm.size()-1), 
      m_bucket_weight_dist(0.0, 1.0), m_rng(comm) {
    if (seed) {
      m_rng = default_random_engine<>(comm, *seed);
    }
    c.for_all([&](const auto& id_weight_pair){
      m_local_items.emplace_back(std::get<0>(id_weight_pair), std::get<1>(id_weight_pair));
    });
    build_alias_table();
  }

  template <typename YGMContainer>
  requires ygm::container::detail::HasForAll<YGMContainer> &&
           ygm::container::detail::DoubleItemTuple<typename YGMContainer::for_all_args> &&
           pair_like_and_convertible_to_weighted_item<Item, typename YGMContainer::for_all_args>
  alias_table(ygm::comm &comm, YGMContainer &c, 
    std::optional<std::uint32_t> seed = std::nullopt) 
    : m_comm(comm), pthis(this), m_rank_dist(0, comm.size()-1), 
      m_bucket_weight_dist(0.0, 1.0), m_rng(comm) {
    if (seed) {
      m_rng = default_random_engine<>(comm, *seed);
    }
    c.for_all([&](const auto &id, const auto &weight){
      m_local_items.emplace_back(id,weight);
    });
    build_alias_table();
  }

  template <typename STLContainer>
  requires ygm::container::detail::STLContainer<STLContainer> &&
           pair_like_and_convertible_to_weighted_item<
            Item, typename STLContainer::value_type>
  alias_table(ygm::comm &comm, STLContainer &c, 
    std::optional<std::uint32_t> seed = std::nullopt) 
    : m_comm(comm), pthis(this), m_rank_dist(0, comm.size()-1),
      m_bucket_weight_dist(0.0, 1.0), m_rng(comm) {  
    if (seed) {
      m_rng = default_random_engine<>(comm, *seed);
    }
    for (const auto& [id, weight] : c) {
      m_local_items.emplace_back(id, weight);
    }
    build_alias_table();
  }

 private:

  void build_alias_table() {
    m_comm.barrier();
    balance_weight();
    m_comm.barrier();
    build_local_alias_table();
    m_local_items.clear();
  }

  void balance_weight() { 
    double local_weight = 0.0;
    for (uint32_t i = 0; i < m_local_items.size(); i++) {
      local_weight += m_local_items[i].weight;
    }
    double global_weight = ygm::sum(local_weight, m_comm);
    double prfx_sum_weight = ygm::prefix_sum(local_weight, m_comm);

    // target_weight = Amount of weight each rank should have after balancing
    double target_weight = global_weight / m_comm.size();
    int dest_rank = prfx_sum_weight / target_weight; 
    // Spillage weight i.e. weight being contributed by other processors to dest's rank local distribution
    double curr_weight = std::fmod(prfx_sum_weight, target_weight); 

    std::vector<item> new_local_items;
    using ygm_items_ptr = ygm::ygm_ptr<std::vector<item>>;
    ygm_items_ptr ptr_new_items = m_comm.make_ygm_ptr(new_local_items); 
    m_comm.barrier();

    std::vector<item> items_to_send;
    // WARNING: size of m_local_items can grow during loop. Do not use iterators or pointers in the loop.
    for (uint64_t i = 0; i < m_local_items.size(); i++) { 
      item local_item = m_local_items[i]; 
      if (curr_weight + local_item.weight >= target_weight) { 
        double remaining_weight = curr_weight + local_item.weight - target_weight;
        double weight_to_send = local_item.weight - remaining_weight;
        curr_weight += weight_to_send;
        item item_to_send = {local_item.id, weight_to_send};
        items_to_send.push_back(item_to_send);

        if (dest_rank < m_comm.size()) {
          // Moves weights to dest_rank's new_local_items
          m_comm.async(dest_rank, [](std::vector<item> items, ygm_items_ptr new_items_ptr) {
            new_items_ptr->insert(new_items_ptr->end(), items.begin(), items.end()); 
          }, items_to_send, ptr_new_items);
        }

        // Handle case where item weight is large enough to span multiple rank's alias tables
        if (remaining_weight >= target_weight) { 
          m_local_items.push_back({local_item.id, remaining_weight});
          curr_weight = 0;
        }  else {
          curr_weight = remaining_weight;
        }
        items_to_send.clear();
        if (curr_weight != 0) {
          items_to_send.push_back({local_item.id, curr_weight});
        }
        dest_rank++;
      } else {
        items_to_send.push_back(local_item);
        curr_weight += local_item.weight;
      }
    }
    
    // Need to handle items left in items to send. Must also account for floating point errors.
    if (items_to_send.size() > 0 && dest_rank < m_comm.size()) {
      m_comm.async(dest_rank, [](std::vector<item> items, ygm_items_ptr new_items_ptr) {
        new_items_ptr->insert(new_items_ptr->end(), items.begin(), items.end()); 
      }, items_to_send, ptr_new_items);
    }

    m_comm.barrier();
    std::swap(new_local_items, m_local_items);

    YGM_ASSERT_RELEASE(m_local_items.size() > 0);
    YGM_ASSERT_RELEASE(is_balanced(target_weight));
  } 

  bool is_balanced(double target) {
    double local_weight = 0.0;
    for (const auto& itm : m_local_items) {
      local_weight += itm.weight;
    } 
    double dif = std::abs(target - local_weight);
    YGM_ASSERT_RELEASE(dif < 1e-6);  

    m_comm.barrier();
    auto equal = [this](double a, double b){
      return (std::abs(a - b) < 1e-6);
    }; 
    bool balanced = ygm::is_same(local_weight, m_comm, equal);
    return balanced;
  }

  void build_local_alias_table() {
    double local_weight = 0.0;
    for (const auto& itm : m_local_items) {
      local_weight += itm.weight;
    } 
    double avg_weight = local_weight / m_local_items.size(); 

    // Implementation of Vose's algorithm, utilized Keith Schwarz numerically stable version
    // https://www.keithschwarz.com/darts-dice-coins/
    std::vector<item> heavy_items;
    std::vector<item> light_items;
    for (auto& itm : m_local_items) {
      if (itm.weight < avg_weight) {
          light_items.push_back(itm);
      } else {
          heavy_items.push_back(itm);
      }
    }

    while (!light_items.empty() && !heavy_items.empty()) {
      item& l = light_items.back();
      item& h = heavy_items.back(); 
      table_item tbl_itm = {l.weight, l.id, h.id};
      m_local_alias_table.push_back(tbl_itm);
      h.weight = (h.weight + l.weight) - avg_weight;
      light_items.pop_back(); 
      if (h.weight < avg_weight) {
        light_items.push_back(h);
        heavy_items.pop_back();
      }   
    }

    // Either heavy items or light_items is empty, need to flush the non empty 
    // vector and add them to the alias table with a p value of avg_weight
    while (!heavy_items.empty()) {
      item& h = heavy_items.back();
      table_item tbl_itm = {avg_weight, h.id, Item()};
      m_local_alias_table.push_back(tbl_itm);
      heavy_items.pop_back();
    }
    while (!light_items.empty()) {
      item& l = light_items.back();
      table_item tbl_itm = {avg_weight, l.id, Item()};
      m_local_alias_table.push_back(tbl_itm);
      light_items.pop_back();
    }
    m_comm.barrier();
    m_num_items_uniform_dist = std::uniform_int_distribution<uint64_t>(0,m_local_alias_table.size()-1);
    m_bucket_weight_dist = std::uniform_real_distribution<double>(0,avg_weight);
    m_avg_weight = avg_weight;
  }
  
 public:

  template <typename Visitor, typename... VisitorArgs>
  void async_sample(Visitor&& visitor, const VisitorArgs &...args) {

    auto sample_wrapper = [visitor](auto ptr_a_tbl, const VisitorArgs &...args) {
      table_item tbl_itm = ptr_a_tbl->m_local_alias_table[ptr_a_tbl->m_num_items_uniform_dist(ptr_a_tbl->m_rng)];
      Item s = tbl_itm.a;
      if (tbl_itm.p < ptr_a_tbl->m_avg_weight) {
        double f = ptr_a_tbl->m_bucket_weight_dist(ptr_a_tbl->m_rng);
        if (f > tbl_itm.p) {
          s = tbl_itm.b;
        }
      }
      ygm::meta::apply_optional(visitor, std::make_tuple(ptr_a_tbl), std::forward_as_tuple(s, args...));
    };

    uint32_t dest_rank = m_rank_dist(m_rng);
    m_comm.async(dest_rank, sample_wrapper, pthis, std::forward<const VisitorArgs>(args)...);
  }        

 private:
  ygm::comm&                                          m_comm;
  ygm::ygm_ptr<self_type>                             pthis;
  std::vector<item>                                   m_local_items;
  std::vector<table_item>                             m_local_alias_table;
  std::uniform_int_distribution<uint32_t>             m_rank_dist;
  std::uniform_int_distribution<uint64_t>             m_num_items_uniform_dist;
  std::uniform_real_distribution<double>              m_zero_one_dist;
  std::uniform_real_distribution<double>              m_bucket_weight_dist; 
  double                                              m_avg_weight;
  ygm::random::default_random_engine<>                m_rng;
};

}  // namespace ygm::random