// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <queue>
#include <stack>

namespace ygm::container::detail {

  /**
   * @brief Queue policy for priority ordering
   */
  template <typename Item, typename Comp>
  struct priority_policy {
    using queue_type = std::priority_queue<Item, std::vector<Item>, Comp>;
    using container_type = ygm::container::work_queue_tag;
    
    static void push(queue_type& q, const Item& item) {
      q.push(item);
    }
    
    static Item top(queue_type& q) {
      return q.top();
    }
    
    static void pop(queue_type& q) {
      q.pop();
    }
    
    static bool empty(const queue_type& q) {
      return q.empty();
    }
    
    static size_t size(const queue_type& q) {
      return q.size();
    }
  };
  
  /**
   * @brief Queue policy for FIFO ordering
   */
  template <typename Item>
  struct fifo_policy {
    using queue_type = std::queue<Item>;
    using container_type = ygm::container::work_queue_tag;
    
    static void push(queue_type& q, const Item& item) {
      q.push(item);
    }
    
    static Item top(queue_type& q) {
      return q.front();
    }
    
    static void pop(queue_type& q) {
      q.pop();
    }
    
    static bool empty(const queue_type& q) {
      return q.empty();
    }
    
    static size_t size(const queue_type& q) {
      return q.size();
    }
  };

  /**
   * @brief Queue policy for LIFO ordering
   */
  template <typename Item>
  struct lifo_policy {
    using queue_type = std::stack<Item>;
    using container_type = ygm::container::work_queue_tag;
    
    static void push(queue_type& q, const Item& item) {
      q.push(item);
    }
    
    static Item top(queue_type& q) {
      return q.top();
    }
    
    static void pop(queue_type& q) {
      q.pop();
    }
    
    static bool empty(const queue_type& q) {
      return q.empty();
    }
    
    static size_t size(const queue_type& q) {
      return q.size();
    }
  };
} // namespace detail