// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <concepts>
#include <random>

#include <ygm/comm.hpp>
#include <ygm/container/container_traits.hpp>
#include <ygm/container/detail/base_async_insert.hpp>
#include <ygm/container/detail/base_async_reduce.hpp>
#include <ygm/container/detail/base_async_visit.hpp>
#include <ygm/container/detail/base_concepts.hpp>
#include <ygm/container/detail/base_iteration.hpp>
#include <ygm/container/detail/base_misc.hpp>
#include <ygm/container/detail/block_partitioner.hpp>

namespace ygm::container {

template <typename Value, typename Index = size_t>
class array
    : public detail::base_async_insert_key_value<array<Value, Index>,
                                                 std::tuple<Index, Value>>,
      public detail::base_misc<array<Value, Index>, std::tuple<Index, Value>>,
      public detail::base_async_visit<array<Value, Index>,
                                      std::tuple<Index, Value>>,
      public detail::base_iteration_key_value<array<Value, Index>,
                                              std::tuple<Index, Value>>,
      public detail::base_async_reduce<array<Value, Index>,
                                       std::tuple<Index, Value>> {
  friend class detail::base_misc<array<Value, Index>, std::tuple<Index, Value>>;

 public:
  using self_type      = array<Value, Index>;
  using mapped_type    = Value;
  using key_type       = Index;
  using size_type      = Index;
  using for_all_args   = std::tuple<Index, Value>;
  using container_type = ygm::container::array_tag;
  using ptr_type       = typename ygm::ygm_ptr<self_type>;

  // Pull in async_visit and async_insert for use within the array
  using detail::base_async_visit<array<Value, Index>,
                                 std::tuple<Index, Value>>::async_visit;
  using detail::base_async_insert_key_value<array<Value, Index>,
                                            for_all_args>::async_insert;

  array() = delete;

  array(ygm::comm& comm, const size_type size)
      : m_comm(comm),
        pthis(this),
        m_global_size(size),
        m_default_value{},
        partitioner(comm, size) {
    m_comm.log(log_level::info, "Creating ygm::container::array");
    pthis.check(m_comm);

    resize(size);
  }

  array(ygm::comm& comm, const size_type size, const mapped_type& default_value)
      : m_comm(comm),
        pthis(this),
        m_global_size(size),
        m_default_value(default_value),
        partitioner(comm, size) {
    m_comm.log(log_level::info, "Creating ygm::container::array");
    pthis.check(m_comm);

    resize(size);
  }

  array(ygm::comm& comm, std::initializer_list<mapped_type> l)
      : m_comm(comm),
        pthis(this),
        m_global_size(l.size()),
        m_default_value{},
        partitioner(comm, l.size()) {
    m_comm.cout0("initializer_list assumes all ranks are equal");
    m_comm.log(log_level::info, "Creating ygm::container::array");
    pthis.check(m_comm);

    resize(l.size());
    if (m_comm.rank0()) {
      key_type index{0};
      for (const mapped_type& value : l) {
        async_insert(index++, value);
      }
    }

    m_comm.barrier();
  }

  array(ygm::comm&                                               comm,
        std::initializer_list<std::tuple<key_type, mapped_type>> l)
      : m_comm(comm), pthis(this), m_default_value{}, partitioner(comm, 0) {
    m_comm.cout0("initializer_list assumes all ranks are equal");
    m_comm.log(log_level::info, "Creating ygm::container::array");
    pthis.check(m_comm);

    key_type max_index{0};
    for (const auto& [index, value] : l) {
      YGM_ASSERT_RELEASE(index >= 0);
      max_index = std::max<key_type>(max_index, index);
    }

    m_global_size = max_index + 1;
    resize(max_index + 1);

    if (m_comm.rank0()) {
      for (const auto& [index, value] : l) {
        async_insert(index, value);
      }
    }

    m_comm.barrier();
  }

  template <typename T>
  array(ygm::comm& comm, const T& t)
    requires detail::HasForAll<T> &&
                 detail::SingleItemTuple<typename T::for_all_args> &&
                 std::same_as<typename T::for_all_args, std::tuple<mapped_type>>
      : m_comm(comm), pthis(this), m_default_value{}, partitioner(comm, 0) {
    m_comm.log(log_level::info, "Creating ygm::container::array");
    pthis.check(m_comm);

    resize(t.size());

    key_type local_index = prefix_sum(t.local_size(), m_comm);

    t.for_all([this, &local_index](const auto& value) {
      async_insert(local_index++, value);
    });

    m_comm.barrier();
  }

  template <typename T>
  array(ygm::comm& comm, const T& t)
    requires detail::HasForAll<T> &&
                 detail::SingleItemTuple<typename T::for_all_args> &&
                 detail::DoubleItemTuple<
                     std::tuple_element_t<0, typename T::for_all_args>> &&
                 std::convertible_to<
                     std::tuple_element_t<
                         0, std::tuple_element_t<0, typename T::for_all_args>>,
                     key_type> &&
                 std::convertible_to<
                     std::tuple_element_t<
                         1, std::tuple_element_t<0, typename T::for_all_args>>,
                     mapped_type>
      : m_comm(comm), pthis(this), m_default_value{}, partitioner(comm, 0) {
    m_comm.log(log_level::info, "Creating ygm::container::array");
    pthis.check(m_comm);

    key_type max_index{0};
    t.for_all([&max_index](const auto& index_value) {
      max_index = std::max<mapped_type>(std::get<0>(index_value), max_index);
    });

    max_index = ::ygm::max(max_index, m_comm);

    resize(max_index + 1);

    t.for_all([this](const auto& index_value) {
      async_insert(std::get<0>(index_value), std::get<1>(index_value));
    });

    m_comm.barrier();
  }

  template <typename T>
  array(ygm::comm& comm, const T& t)
    requires detail::HasForAll<T> &&
                 detail::DoubleItemTuple<typename T::for_all_args> &&
                 std::convertible_to<
                     std::tuple_element_t<0, typename T::for_all_args>,
                     key_type> &&
                 std::convertible_to<
                     std::tuple_element_t<0, typename T::for_all_args>,
                     mapped_type>
      : m_comm(comm), pthis(this), m_default_value{}, partitioner(comm, 0) {
    m_comm.log(log_level::info, "Creating ygm::container::array");
    pthis.check(m_comm);

    key_type max_index{0};
    t.for_all([&max_index](const auto& index, const auto& value) {
      max_index = std::max<mapped_type>(index, max_index);
    });

    max_index = ::ygm::max(max_index, m_comm);

    resize(max_index + 1);

    t.for_all([this](const auto& index, const auto& value) {
      async_insert(index, value);
    });

    m_comm.barrier();
  }

  template <typename T>
  array(ygm::comm& comm, const T& t)
    requires detail::STLContainer<T> &&
                 (not detail::SingleItemTuple<typename T::value_type>) &&
                 std::convertible_to<typename T::value_type, mapped_type>
      : m_comm(comm), pthis(this), m_default_value{}, partitioner(comm, 0) {
    m_comm.log(log_level::info, "Creating ygm::container::array");
    pthis.check(m_comm);

    auto global_size = sum(t.size(), m_comm);
    resize(global_size);

    key_type local_index = prefix_sum(t.size(), m_comm);

    std::for_each(t.cbegin(), t.cend(),
                  [this, &local_index](const auto& value) {
                    async_insert(local_index++, value);
                  });

    m_comm.barrier();
  }

  template <typename T>
  array(ygm::comm& comm, const T& t)
    requires detail::STLContainer<T> &&
                 detail::DoubleItemTuple<typename T::value_type> &&
                 std::convertible_to<
                     std::tuple_element_t<0, typename T::value_type>,
                     key_type> &&
                 std::convertible_to<
                     std::tuple_element_t<1, typename T::value_type>,
                     mapped_type>
      : m_comm(comm), pthis(this), m_default_value{}, partitioner(comm, 0) {
    m_comm.log(log_level::info, "Creating ygm::container::array");
    pthis.check(m_comm);

    key_type max_index{0};
    std::for_each(t.begin(), t.end(), [&max_index](const auto& index_value) {
      max_index = std::max<key_type>(std::get<0>(index_value), max_index);
    });

    max_index = ::ygm::max(max_index, m_comm);

    resize(max_index + 1);

    std::for_each(t.cbegin(), t.cend(), [this](const auto& index_value) {
      async_insert(std::get<0>(index_value), std::get<1>(index_value));
    });
  }

  ~array() {
    m_comm.barrier();
    m_comm.log(log_level::info, "Destroying ygm::container::array");
  }

  array(const self_type& other)
      : m_comm(other.comm()),
        pthis(this),
        m_global_size(other.m_global_size),
        m_default_value(other.m_default_value),
        m_local_vec(other.m_local_vec),
        partitioner(other.m_comm, other.m_global_size) {
    m_comm.log(log_level::info, "Copying ygm::container::array");
    pthis.check(m_comm);
  }

  array(self_type&& other) noexcept
      : m_comm(other.comm()),
        pthis(this),
        m_global_size(other.m_global_size),
        m_default_value(other.m_default_value),
        m_local_vec(std::move(other.m_local_vec)),
        partitioner(other.comm(), other.m_global_size) {
    m_comm.log(log_level::info, "Moving ygm::container::array");
    pthis.check(m_comm);

    other.m_global_size = 0;
  }

  array& operator=(const self_type& other) {
    m_comm.log(log_level::info,
               "Calling ygm::container::array copy assignment operator");
    resize(other.m_global_size);
    m_default_value = other.m_default_value;
    m_local_vec     = other.m_local_vec;

    return *this;
  }

  array& operator=(self_type&& other) noexcept {
    m_comm.log(log_level::info,
               "Calling ygm::container::array move assignment operator");
    m_global_size   = other.m_global_size;
    m_default_value = other.m_default_value;
    partitioner = detail::block_partitioner<key_type>(m_comm, m_global_size);

    std::swap(m_local_vec, other.m_local_vec);

    if (other.m_local_vec.size() > 0) {
      other.m_local_vec.clear();
    }
    other.m_global_size = 0;

    return *this;
  }

  void local_insert(const key_type& key, const mapped_type& value) {
    m_local_vec[partitioner.local_index(key)] = value;
  }

  template <typename Function, typename... VisitorArgs>
  void local_visit(const key_type index, Function&& fn,
                   const VisitorArgs&... args) {
    ygm::detail::interrupt_mask mask(m_comm);
    if constexpr (std::is_invocable<decltype(fn), const key_type, mapped_type&,
                                    VisitorArgs&...>() ||
                  std::is_invocable<decltype(fn), ptr_type, const key_type,
                                    mapped_type&, VisitorArgs&...>()) {
      ygm::meta::apply_optional(
          std::forward<Function>(fn), std::make_tuple(pthis),
          std::forward_as_tuple(
              index, m_local_vec[partitioner.local_index(index)], args...));
    } else {
      static_assert(ygm::detail::always_false<Function>,
                    "remote array lambda must be "
                    "invocable with (const "
                    "key_type, mapped_type &, ...) or "
                    "(ptr_type, mapped_type &, ...) signatures");
    }
  }

  void async_set(const key_type index, const mapped_type& value) {
    async_insert(index, value);
  }

  template <typename BinaryOp>
  void async_binary_op_update_value(const key_type     index,
                                    const mapped_type& value,
                                    const BinaryOp&    b) {
    YGM_ASSERT_RELEASE(index < m_global_size);
    auto updater = [](const key_type i, mapped_type& v,
                      const mapped_type& new_value) {
      BinaryOp* binary_op;
      v = (*binary_op)(v, new_value);
    };

    async_visit(index, updater, value);
  }

  void async_bit_and(const key_type index, const mapped_type& value) {
    async_binary_op_update_value(index, value, std::bit_and<mapped_type>());
  }

  void async_bit_or(const key_type index, const mapped_type& value) {
    async_binary_op_update_value(index, value, std::bit_or<mapped_type>());
  }

  void async_bit_xor(const key_type index, const mapped_type& value) {
    async_binary_op_update_value(index, value, std::bit_xor<mapped_type>());
  }

  void async_logical_and(const key_type index, const mapped_type& value) {
    async_binary_op_update_value(index, value, std::logical_and<mapped_type>());
  }

  void async_logical_or(const key_type index, const mapped_type& value) {
    async_binary_op_update_value(index, value, std::logical_or<mapped_type>());
  }

  void async_multiplies(const key_type index, const mapped_type& value) {
    async_binary_op_update_value(index, value, std::multiplies<mapped_type>());
  }

  void async_divides(const key_type index, const mapped_type& value) {
    async_binary_op_update_value(index, value, std::divides<mapped_type>());
  }

  void async_plus(const key_type index, const mapped_type& value) {
    async_binary_op_update_value(index, value, std::plus<mapped_type>());
  }

  void async_minus(const key_type index, const mapped_type& value) {
    async_binary_op_update_value(index, value, std::minus<mapped_type>());
  }

  template <typename UnaryOp>
  void async_unary_op_update_value(const key_type index, const UnaryOp& u) {
    YGM_ASSERT_RELEASE(index < m_global_size);
    auto updater = [](const key_type i, mapped_type& v) {
      UnaryOp* u;
      v = (*u)(v);
    };

    async_visit(index, updater);
  }

  void async_increment(const key_type index) {
    async_unary_op_update_value(index,
                                [](const mapped_type& v) { return v + 1; });
  }

  void async_decrement(const key_type index) {
    async_unary_op_update_value(index,
                                [](const mapped_type& v) { return v - 1; });
  }

  const mapped_type& default_value() const;

  void resize(const size_type size, const mapped_type& fill_value) {
    m_comm.barrier();

    // Copy current values into temporary vector for storing in
    // ygm::container::array after resizing local array structures
    std::vector<std::pair<const key_type, const mapped_type>> tmp_values;
    tmp_values.reserve(local_size());
    local_for_all(
        [&tmp_values, size](const key_type& index, const mapped_type& value) {
          if (index < size) {
            tmp_values.push_back(std::make_pair(index, value));
          }
        });

    m_global_size = size;
    partitioner   = detail::block_partitioner<key_type>(m_comm, size);

    m_local_vec.resize(partitioner.local_size(), fill_value);

    m_default_value = fill_value;

    // Repopulate array values
    for (const auto& [index, value] : tmp_values) {
      async_set(index, value);
    }

    m_comm.barrier();
  }

  void resize(const size_type size) { resize(size, m_default_value); }

  size_t local_size() { return partitioner.local_size(); }

  size_t size() const {
    m_comm.barrier();
    return m_global_size;
  }

  void local_clear() { resize(0); }

  void local_swap(self_type& other) {
    m_local_vec.swap(other.m_local_vec);
    std::swap(m_global_size, other.m_global_size);
    std::swap(m_default_value, other.m_default_value);
    std::swap(partitioner, other.partitioner);
  }

  template <typename Function>
  void local_for_all(Function&& fn) {
    if constexpr (std::is_invocable<decltype(fn), const key_type,
                                    mapped_type&>()) {
      for (int i = 0; i < m_local_vec.size(); ++i) {
        key_type g_index = partitioner.global_index(i);
        fn(g_index, m_local_vec[i]);
      }
    } else if constexpr (std::is_invocable<decltype(fn), mapped_type&>()) {
      std::for_each(std::begin(m_local_vec), std::end(m_local_vec),
                    std::forward<Function>(fn));
    } else {
      static_assert(ygm::detail::always_false<Function>,
                    "local array lambda must be "
                    "invocable with (const "
                    "key_type, mapped_type &) or "
                    "(mapped_type &) signatures");
    }
  }

  template <typename ReductionOp>
  void local_reduce(const key_type index, const mapped_type& value,
                    ReductionOp reducer) {
    m_local_vec[partitioner.local_index(index)] =
        reducer(value, m_local_vec[partitioner.local_index(index)]);
  }

  void sort() {
    const key_type samples_per_pivot = std::max<key_type>(
        std::min<key_type>(20, m_global_size / m_comm.size()), 1);
    std::vector<mapped_type> to_sort;
    to_sort.reserve(local_size() * 1.1f);

    //
    //  Choose pivots, uses index as 3rd sorting argument to solve issue with
    //  lots of duplicate items
    std::vector<std::pair<mapped_type, key_type>> samples;
    std::vector<std::pair<mapped_type, key_type>> pivots;
    static auto&                                  s_samples = samples;
    static auto&                                  s_to_sort = to_sort;
    samples.reserve((m_comm.size() - 1) * samples_per_pivot);

    std::default_random_engine rng;

    std::uniform_int_distribution<size_t> uintdist{0, size() - 1};

    for (size_t i = 0; i < samples_per_pivot * (m_comm.size() - 1); ++i) {
      size_t index = uintdist(rng);
      if (index >= partitioner.local_start() &&
          index < partitioner.local_start() + partitioner.local_size()) {
        m_comm.async_bcast(
            [](const std::pair<mapped_type, key_type>& sample) {
              s_samples.push_back(sample);
            },
            std::make_pair(m_local_vec[index - partitioner.local_start()],
                           index));
      }
    }
    m_comm.barrier();

    YGM_ASSERT_RELEASE(samples.size() ==
                       samples_per_pivot * (m_comm.size() - 1));
    std::sort(samples.begin(), samples.end());
    for (size_t i = samples_per_pivot - 1; i < samples.size();
         i += samples_per_pivot) {
      pivots.push_back(samples[i]);
    }
    samples.clear();
    samples.shrink_to_fit();

    YGM_ASSERT_RELEASE(pivots.size() == m_comm.size() - 1);

    //
    // Partition using pivots
    for (size_t i = 0; i < m_local_vec.size(); ++i) {
      auto itr = std::lower_bound(
          pivots.begin(), pivots.end(),
          std::make_pair(m_local_vec[i], partitioner.local_start() + i));
      size_t owner = std::distance(pivots.begin(), itr);

      m_comm.async(
          owner, [](const mapped_type& val) { s_to_sort.push_back(val); },
          m_local_vec[i]);
    }
    m_comm.barrier();

    if (not to_sort.empty()) {
      std::sort(to_sort.begin(), to_sort.end());
    }

    size_t my_prefix = ygm::prefix_sum(to_sort.size(), m_comm);
    for (key_type i = 0; i < to_sort.size(); ++i) {
      async_insert(my_prefix + i, to_sort[i]);
    }

    m_comm.barrier();
  }

  detail::block_partitioner<key_type> partitioner;

 private:
  size_type                        m_global_size;
  mapped_type                      m_default_value;
  std::vector<mapped_type>         m_local_vec;
  ygm::comm&                       m_comm;
  typename ygm::ygm_ptr<self_type> pthis;
};

}  // namespace ygm::container
