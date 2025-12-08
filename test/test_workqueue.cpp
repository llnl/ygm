// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG

#include <ygm/comm.hpp>
#include <ygm/container/workqueue.hpp>
#include <ygm/container/array.hpp>

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
      size_t size_max = 64;

      std::vector<size_t> work_items(size_max);
      std::iota(work_items.begin(), work_items.end(), 0);
      
      auto rng = std::default_random_engine {};
      std::ranges::shuffle(work_items, rng);

      auto work_lambda = [&size_max] (auto p_work_queue, auto& queued_item) {
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
      size_t size_max = 64;

      std::vector<size_t> work_items(size_max);
      std::iota(work_items.begin(), work_items.end(), 0);

      auto work_lambda = [&size_max] (auto p_work_queue, auto& queued_item) {
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

    // test recursive calls with priority ordering
    {
      size_t cutoff = 64;
      bool found_cutoff = false;

      size_t xref = 0;

      auto work_lambda = [&cutoff, &found_cutoff, &xref] (auto p_work_queue, auto& queued_item) {
        if (queued_item < cutoff) {
          YGM_ASSERT_RELEASE(found_cutoff == false);
          YGM_ASSERT_RELEASE(xref == queued_item);
          xref++;

          p_work_queue->local_insert(queued_item + cutoff + 1);
          p_work_queue->local_insert(queued_item + 1);
        } 
        else {
          found_cutoff = true;
        }
      };

      auto wq = ygm::container::make_priority_workqueue<size_t, std::greater<size_t>> (world, work_lambda);

      wq.local_insert(0);

      world.barrier();
    }

    // test container traversal
    {
      int                               size = 64;
      ygm::container::array<int> arr(world, size);

      if (world.rank0()) {
        for (int i = 0; i < size; ++i) {
          arr.async_set(i, i);
        }
      }

      world.barrier();


      auto recv_enqueue_lambda = [size] (const auto& ind, int &val, auto p_wq) {
        if (val < size -1) {
          p_wq->local_insert(val + 1);
        };

        val = 0;
      };

      auto work_lambda = [&arr, &recv_enqueue_lambda] (auto p_wq, int item) {
        arr.async_visit(item, recv_enqueue_lambda, p_wq);
      };

      auto wq = ygm::container::make_priority_workqueue<int, std::greater<int>> (world, work_lambda);

      if (world.rank0()) {
        wq.local_insert(0);
      }

      world.barrier();

      arr.for_all([] (const auto value) {
        YGM_ASSERT_RELEASE(value == 0);
      });

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

    // test fifo ordering with recursion
    {
      size_t cutoff = 64;
      size_t mod = 8;
      size_t xref = 0;

      auto work_lambda = [&cutoff, &mod, &xref] (auto p_work_queue, auto& queued_item) {
        
        YGM_ASSERT_RELEASE(queued_item == xref);
        
        if (queued_item == cutoff) {
          return;
        }
        
        if (queued_item % mod == 0) {
          for (size_t i = 1; i <= mod; i++) {
            p_work_queue->local_insert(queued_item + i);
          } 
        }

        xref++;
      };

      auto wq = ygm::container::make_fifo_workqueue<size_t> (world, work_lambda);

      wq.local_insert(0);

      world.barrier();

      YGM_ASSERT_RELEASE(xref == cutoff);
    }

    // test container traversal
    {
      int                               size = 64;
      ygm::container::array<int> arr(world, size);

      if (world.rank0()) {
        for (int i = 0; i < size; ++i) {
          arr.async_set(i, i);
        }
      }

      world.barrier();


      auto recv_enqueue_lambda = [size] (const auto& ind, int &val, auto p_wq) {
        if (val < size -1) {
          p_wq->local_insert(val + 1);
        };

        val = 0;
      };

      auto work_lambda = [&arr, &recv_enqueue_lambda] (auto p_wq, int item) {
        arr.async_visit(item, recv_enqueue_lambda, p_wq);
      };

      auto wq = ygm::container::make_fifo_workqueue<int> (world, work_lambda);

      if (world.rank0()) {
        wq.local_insert(0);
      }

      world.barrier();

      arr.for_all([] (const auto value) {
        YGM_ASSERT_RELEASE(value == 0);
      });

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

    // test lifo ordering with recursion
    {
      size_t cutoff = 64;
      size_t mod = 8;
      size_t xref = 0;

      auto work_lambda = [&cutoff, &mod, &xref] (auto p_work_queue, auto& queued_item) {
        
        YGM_ASSERT_RELEASE(queued_item == xref);
        
        if (queued_item == cutoff) {
          return;
        }
        
        if (queued_item % mod == 0) {
          for (size_t i = mod; i > 0; i--) {
            p_work_queue->local_insert(queued_item + i);
          } 
        }

        xref++;
      };

      auto wq = ygm::container::make_lifo_workqueue<size_t> (world, work_lambda);

      wq.local_insert(0);

      world.barrier();

      YGM_ASSERT_RELEASE(xref == cutoff);
    }

    // test container traversal
    {
      int                               size = 64;
      ygm::container::array<int> arr(world, size);

      if (world.rank0()) {
        for (int i = 0; i < size; ++i) {
          arr.async_set(i, i);
        }
      }

      world.barrier();


      auto recv_enqueue_lambda = [size] (const auto& ind, int &val, auto p_wq) {
        if (val < size -1) {
          p_wq->local_insert(val + 1);
        };

        val = 0;
      };

      auto work_lambda = [&arr, &recv_enqueue_lambda] (auto p_wq, int item) {
        arr.async_visit(item, recv_enqueue_lambda, p_wq);
      };

      auto wq = ygm::container::make_lifo_workqueue<int> (world, work_lambda);

      if (world.rank0()) {
        wq.local_insert(0);
      }

      world.barrier();

      arr.for_all([] (const auto value) {
        YGM_ASSERT_RELEASE(value == 0);
      });

      world.barrier();
    }
  }

  return 0;
}
