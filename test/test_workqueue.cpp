// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG

#include <ygm/comm.hpp>
#include <ygm/container/workqueue.hpp>

#include <vector>
#include <algorithm>
#include <numeric>
#include <random>

int main(int argc, char **argv) {
  ygm::comm world(&argc, &argv);

  // priority queue tests
  {
    // test local priority workqueue ordering and size checks
    {
      static size_t size_max = 64;

      std::vector<size_t> work_items(size_max);
      std::iota(work_items.begin(), work_items.end(), 0);
      
      auto rng = std::default_random_engine {};
      std::ranges::shuffle(work_items, rng);

      auto work_lambda = [] (auto p_work_queue, auto& queued_item) {
        size_max--;
        YGM_ASSERT_RELEASE(size_max == queued_item);
        YGM_ASSERT_RELEASE(size_max == p_work_queue->local_size());
      };

      auto wq = ygm::container::make_priority_workqueue<size_t, std::less<size_t>> (world, work_lambda);

      for (size_t item : work_items) {
        wq.local_insert(item);
      }

      YGM_ASSERT_RELEASE(wq.local_has_work() == true);

      world.barrier();

      YGM_ASSERT_RELEASE(size_max == 0);
      YGM_ASSERT_RELEASE(wq.local_size() == 0);
      YGM_ASSERT_RELEASE(wq.local_has_work() == false);

      world.barrier();
    }

    // test local_clear
    {
      static size_t size_max = 64;

      std::vector<size_t> work_items(size_max);
      std::iota(work_items.begin(), work_items.end(), 0);

      auto work_lambda = [] (auto p_work_queue, auto& queued_item) {
        size_max += queued_item;
      };

      auto wq = ygm::container::make_priority_workqueue<size_t, std::less<size_t>> (world, work_lambda);

      for (size_t item : work_items) {
        wq.local_insert(item);
      }

      YGM_ASSERT_RELEASE(wq.local_size() == size_max);
      YGM_ASSERT_RELEASE(wq.local_has_work() == true);

      wq.local_clear();

      YGM_ASSERT_RELEASE(wq.local_size() == 0);
      YGM_ASSERT_RELEASE(wq.local_has_work() == false);

      world.barrier();
    }
  }


  // FIFO queue tests
  {
    // test local workqueue ordering integrity and size checks
    {
      static size_t size_max = 64;

      std::vector<size_t> work_items(size_max);
      std::iota(work_items.begin(), work_items.end(), 0);
      std::reverse(work_items.begin(), work_items.end());

      auto work_lambda = [] (auto p_work_queue, auto& queued_item) {
        size_max--;
        YGM_ASSERT_RELEASE(size_max == queued_item);
        YGM_ASSERT_RELEASE(size_max == p_work_queue->local_size());
      };

      auto wq = ygm::container::make_fifo_workqueue<size_t> (world, work_lambda);

      for (size_t item : work_items) {
        wq.local_insert(item);
      }

      YGM_ASSERT_RELEASE(wq.local_has_work() == true);

      world.barrier();

      YGM_ASSERT_RELEASE(size_max == 0);
      YGM_ASSERT_RELEASE(wq.local_size() == 0);
      YGM_ASSERT_RELEASE(wq.local_has_work() == false);

      world.barrier();
    }

    // test local_clear
    {
      static size_t size_max = 64;

      std::vector<size_t> work_items(size_max);
      std::iota(work_items.begin(), work_items.end(), 0);

      auto work_lambda = [] (auto p_work_queue, auto& queued_item) {
        size_max += queued_item;
      };

      auto wq = ygm::container::make_fifo_workqueue<size_t> (world, work_lambda);

      for (size_t item : work_items) {
        wq.local_insert(item);
      }

      YGM_ASSERT_RELEASE(wq.local_size() == size_max);
      YGM_ASSERT_RELEASE(wq.local_has_work() == true);

      wq.local_clear();

      YGM_ASSERT_RELEASE(wq.local_size() == 0);
      YGM_ASSERT_RELEASE(wq.local_has_work() == false);

      world.barrier();
    }
  }


  // LIFO queue tests
  {
    // test local LIFO workqueue ordering and size checks
    {
      static size_t size_max = 64;

      std::vector<size_t> work_items(size_max);
      std::iota(work_items.begin(), work_items.end(), 0);

      auto work_lambda = [] (auto p_work_queue, auto& queued_item) {
        size_max--;
        YGM_ASSERT_RELEASE(size_max == queued_item);
        YGM_ASSERT_RELEASE(size_max == p_work_queue->local_size());
      };

      auto wq = ygm::container::make_lifo_workqueue<size_t> (world, work_lambda);

      for (size_t item : work_items) {
        wq.local_insert(item);
      }

      YGM_ASSERT_RELEASE(wq.local_has_work() == true);

      world.barrier();

      YGM_ASSERT_RELEASE(size_max == 0);
      YGM_ASSERT_RELEASE(wq.local_size() == 0);
      YGM_ASSERT_RELEASE(wq.local_has_work() == false);

      world.barrier();
    }

    // test local_clear
    {
      static size_t size_max = 64;

      std::vector<size_t> work_items(size_max);
      std::iota(work_items.begin(), work_items.end(), 0);

      auto work_lambda = [] (auto p_work_queue, auto& queued_item) {
        size_max += queued_item;
      };

      auto wq = ygm::container::make_lifo_workqueue<size_t> (world, work_lambda);

      for (size_t item : work_items) {
        wq.local_insert(item);
      }

      YGM_ASSERT_RELEASE(wq.local_size() == size_max);
      YGM_ASSERT_RELEASE(wq.local_has_work() == true);

      wq.local_clear();

      YGM_ASSERT_RELEASE(wq.local_size() == 0);
      YGM_ASSERT_RELEASE(wq.local_has_work() == false);

      world.barrier();
    }
  }

  return 0;
}
