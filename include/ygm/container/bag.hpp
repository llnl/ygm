// Copyright 2019-2021 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <fstream>

#include <boost/container/deque.hpp>
#include <cereal/archives/json.hpp>
#include <initializer_list>
#include <ygm/container/container_traits.hpp>
#include <ygm/container/detail/base_async_insert.hpp>
#include <ygm/container/detail/base_count.hpp>
#include <ygm/container/detail/base_iteration.hpp>
#include <ygm/container/detail/base_iterators.hpp>
#include <ygm/container/detail/base_misc.hpp>
#include <ygm/container/detail/round_robin_partitioner.hpp>
#include <ygm/random.hpp>

namespace ygm::container {

template <typename Item>
class bag : public detail::base_async_insert_value<bag<Item>, std::tuple<Item>>,
            public detail::base_count<bag<Item>, std::tuple<Item>>,
            public detail::base_misc<bag<Item>, std::tuple<Item>>,
            public detail::base_iterators<bag<Item>>,
            public detail::base_iteration_value<bag<Item>, std::tuple<Item>> {
  friend class detail::base_misc<bag<Item>, std::tuple<Item>>;

  using block_32k_option_t = boost::container::deque_options<
      boost::container::block_size<32 * 1024u>>::type;
  using local_container_type =
      boost::container::deque<Item, void, block_32k_option_t>;

 public:
  using self_type      = bag<Item>;
  using value_type     = Item;
  using size_type      = size_t;
  using for_all_args   = std::tuple<Item>;
  using container_type = ygm::container::bag_tag;
  using iterator       = typename local_container_type::iterator;
  using const_iterator = typename local_container_type::const_iterator;

  bag(ygm::comm &comm) : m_comm(comm), pthis(this), partitioner(comm) {
    m_comm.log(log_level::info, "Creating ygm::container::bag");
    pthis.check(m_comm);
  }

  bag(ygm::comm &comm, std::initializer_list<Item> l)
      : m_comm(comm), pthis(this), partitioner(comm) {
    m_comm.log(log_level::info, "Creating ygm::container::bag");
    pthis.check(m_comm);
    if (m_comm.rank0()) {
      for (const Item &i : l) {
        async_insert(i);
      }
    }
    m_comm.barrier();
  }

  template <typename STLContainer>
  bag(ygm::comm &comm, const STLContainer &cont)
    requires detail::STLContainer<STLContainer> &&
                 std::convertible_to<typename STLContainer::value_type, Item>
      : m_comm(comm), pthis(this), partitioner(comm) {
    m_comm.log(log_level::info, "Creating ygm::container::bag");
    pthis.check(m_comm);

    for (const Item &i : cont) {
      this->async_insert(i);
    }
    m_comm.barrier();
  }

  template <typename YGMContainer>
  bag(ygm::comm &comm, const YGMContainer &yc)
    requires detail::HasForAll<YGMContainer> &&
                 detail::SingleItemTuple<typename YGMContainer::for_all_args>
      : m_comm(comm), pthis(this), partitioner(comm) {
    m_comm.log(log_level::info, "Creating ygm::container::bag");
    pthis.check(m_comm);

    yc.for_all([this](const Item &value) { this->async_insert(value); });

    m_comm.barrier();
  }

  ~bag() {
    m_comm.log(log_level::info, "Destroying ygm::container::bag");
    m_comm.barrier();
  }

  bag(const self_type &other)
      : m_comm(other.comm()),
        pthis(this),
        partitioner(other.comm()),
        m_local_bag(other.m_local_bag) {
    m_comm.log(log_level::info, "Creating ygm::container::bag");
    pthis.check(m_comm);
  }

  bag(self_type &&other) noexcept
      : m_comm(other.comm()),
        pthis(this),
        partitioner(other.comm()),
        m_local_bag(std::move(other.m_local_bag)) {
    m_comm.log(log_level::info, "Creating ygm::container::bag");
    pthis.check(m_comm);
  }

  bag &operator=(const self_type &other) { return *this = bag(other); }

  bag &operator=(self_type &&other) noexcept {
    std::swap(m_local_bag, other.m_local_bag);
    return *this;
  }

  iterator local_begin() { return m_local_bag.begin(); }
  const_iterator local_begin() const { return m_local_bag.cbegin(); }
  const_iterator local_cbegin() const { return m_local_bag.cbegin(); }

  iterator local_end() { return m_local_bag.end(); }
  const_iterator local_end() const { return m_local_bag.cend(); }
  const_iterator local_cend() const { return m_local_bag.cend(); }

  using detail::base_async_insert_value<bag<Item>, for_all_args>::async_insert;

  void async_insert(const Item &value, int dest) {
    auto inserter = [](auto pcont, const value_type &item) {
      pcont->local_insert(item);
    };

    m_comm.async(dest, inserter, this->get_ygm_ptr(), value);
  }

  void async_insert(const std::vector<Item> &values, int dest) {
    auto inserter = [](auto pcont, const std::vector<Item> &values) {
      for (const auto &v : values) {
        pcont->local_insert(v);
      }
    };

    m_comm.async(dest, inserter, this->get_ygm_ptr(), values);
  }

  void local_insert(const Item &val) { m_local_bag.push_back(val); }

  void local_clear() { m_local_bag.clear(); }

  size_t local_size() const { return m_local_bag.size(); }

  size_t local_count(const value_type &val) const {
    return std::count(m_local_bag.begin(), m_local_bag.end(), val);
  }

  template <typename Function>
  void local_for_all(Function &&fn) {
    std::for_each(m_local_bag.begin(), m_local_bag.end(), fn);
  }

  template <typename Function>
  void local_for_all(Function &&fn) const {
    std::for_each(m_local_bag.cbegin(), m_local_bag.cend(), fn);
  }

  void serialize(const std::string &fname) {
    m_comm.barrier();
    std::string   rank_fname = fname + std::to_string(m_comm.rank());
    std::ofstream os(rank_fname, std::ios::binary);
    cereal::JSONOutputArchive oarchive(os);
    // oarchive(m_local_bag, m_round_robin, m_comm.size());
    oarchive(m_local_bag, m_comm.size());
  }

  void deserialize(const std::string &fname) {
    m_comm.barrier();

    std::string   rank_fname = fname + std::to_string(m_comm.rank());
    std::ifstream is(rank_fname, std::ios::binary);

    cereal::JSONInputArchive iarchive(is);
    int                      comm_size;
    // iarchive(m_local_bag, m_round_robin, comm_size);
    iarchive(m_local_bag, comm_size);

    if (comm_size != m_comm.size()) {
      m_comm.cerr0(
          "Attempting to deserialize bag_impl using communicator of "
          "different size than serialized with");
    }
  }

  void rebalance() {
    auto global_size = this->size();  // includes barrier

    // Find current rank's prefix val and desired target size
    size_t prefix_val  = ygm::prefix_sum(local_size(), m_comm);
    size_t target_size = std::ceil((global_size * 1.0) / m_comm.size());

    // Init to_send array where index is dest and value is the num to send
    // int to_send[m_comm.size()] = {0};
    std::unordered_map<size_t, size_t> to_send;

    size_t small_block_size = global_size / m_comm.size();
    size_t large_block_size =
        global_size / m_comm.size() + ((global_size / m_comm.size()) > 0);

    for (size_t i = 0; i < local_size(); i++) {
      size_t idx = prefix_val + i;
      size_t target_rank;

      // Determine target rank to match partitioning in ygm::container::array
      if (idx < (global_size % m_comm.size()) * large_block_size) {
        target_rank = idx / large_block_size;
      } else {
        target_rank = (global_size % m_comm.size()) +
                      (idx - (global_size % m_comm.size()) * large_block_size) /
                          small_block_size;
      }

      if (target_rank != m_comm.rank()) {
        to_send[target_rank]++;
      }
    }
    m_comm.barrier();

    // Build and send bag indexes as calculated by to_send
    for (auto &kv_pair : to_send) {
      async_insert(local_pop(kv_pair.second), kv_pair.first);
    }

    m_comm.barrier();
  }

  template <typename RandomFunc>
  void local_shuffle(RandomFunc &r) {
    m_comm.barrier();
    std::shuffle(m_local_bag.begin(), m_local_bag.end(), r);
  }

  void local_shuffle() {
    ygm::default_random_engine<> r(m_comm, std::random_device()());
    local_shuffle(r);
  }

  template <typename RandomFunc>
  void global_shuffle(RandomFunc &r) {
    m_comm.barrier();
    decltype(m_local_bag) old_local_bag;
    std::swap(old_local_bag, m_local_bag);

    auto send_item = [](auto bag, const value_type &item) {
      bag->m_local_bag.push_back(item);
    };

    std::uniform_int_distribution<> distrib(0, m_comm.size() - 1);
    for (value_type i : old_local_bag) {
      m_comm.async(distrib(r), send_item, pthis, i);
    }
  }

  void global_shuffle() {
    ygm::default_random_engine<> r(m_comm, std::random_device()());
    global_shuffle(r);
  }

  //  private:
  //   template <typename Function∏>
  //   void local_for_all_pair_types(Function fn);

  detail::round_robin_partitioner partitioner;

 private:
  std::vector<value_type> local_pop(int n) {
    YGM_ASSERT_RELEASE(n <= local_size());

    size_t                  new_size  = local_size() - n;
    auto                    pop_start = m_local_bag.begin() + new_size;
    std::vector<value_type> ret;
    ret.assign(pop_start, m_local_bag.end());
    m_local_bag.resize(new_size);
    return ret;
  }

  void local_swap(self_type &other) { m_local_bag.swap(other.m_local_bag); }

  ygm::comm                       &m_comm;
  local_container_type             m_local_bag;
  typename ygm::ygm_ptr<self_type> pthis;
};

}  // namespace ygm::container
