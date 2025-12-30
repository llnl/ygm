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
#include <ygm/container/detail/base_iterators.hpp>
#include <ygm/container/detail/base_misc.hpp>
#include <ygm/container/detail/block_partitioner.hpp>

namespace ygm::container {

/**
 * @brief Container for key-value pairs with keys that are contiguous indices in
 * the range [0, size()-1]
 *
 * @details Assigns ranks contiguous chunks of indices using block_partitioner
 * object. Resizing array is an expensive operation as it requires reassigning
 * storage to ranks.
 *
 */
template <typename Value, typename Index = size_t>
class array
    : public detail::base_async_insert_key_value<array<Value, Index>,
                                                 std::tuple<Index, Value>>,
      public detail::base_misc<array<Value, Index>, std::tuple<Index, Value>>,
      public detail::base_async_visit<array<Value, Index>,
                                      std::tuple<Index, Value>>,
      public detail::base_iterators<array<Value, Index>>,
      public detail::base_iteration_key_value<array<Value, Index>,
                                              std::tuple<Index, Value>>,
      public detail::base_async_reduce<array<Value, Index>,
                                       std::tuple<Index, Value>> {
  friend struct detail::base_misc<array<Value, Index>,
                                  std::tuple<Index, Value>>;

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

  /*
   * @brief Proxy class for returning pair-like objects that contain references
   */
  template <typename T, bool IsConst>
  struct iterator_proxy {
   public:
    using value_ref_type = std::conditional_t<IsConst, const T&, T&>;

    iterator_proxy(const key_type i, value_ref_type v) : index(i), value(v) {};

    /*
     * @brief `get()` method for tuple-like access
     *
     * Allows use of structured bindings
     */
    template <std::size_t N>
    decltype(auto) get() const {
      if constexpr (N == 0)
        return index;
      else if constexpr (N == 1)
        return value;
    }

    const key_type index;
    value_ref_type value;
  };

  /*
   * @brief Iterator for array that gives access to items and their indices
   */
  template <typename T, bool IsConst>
  class array_iterator {
   public:
    using value_type          = std::pair<key_type, mapped_type&>;
    using iterator_proxy_type = iterator_proxy<T, IsConst>;

    array_iterator(self_type* arr, const key_type offset, const key_type index)
        : p_arr(arr), m_offset(offset), m_index(index) {};

    iterator_proxy_type operator*() const {
      return iterator_proxy_type(m_index + m_offset,
                                 p_arr->m_local_vec[m_index]);
    }

    struct arrow_proxy {
      iterator_proxy_type  m_proxy;
      iterator_proxy_type* operator->() { return &m_proxy; }
    };

    arrow_proxy operator->() const { return arrow_proxy{**this}; }

    array_iterator& operator++() {
      m_index++;

      return *this;
    }

    array_iterator operator++(int) {
      iterator tmp(*this);
      ++(*this);
      return tmp;
    }

    bool operator==(const array_iterator& other) {
      return m_index == other.m_index;
    }

    bool operator!=(const array_iterator& other) {
      return m_index != other.m_index;
    }

   private:
    self_type* p_arr;
    key_type   m_offset;
    key_type   m_index;
  };

  using iterator       = array_iterator<mapped_type, false>;
  using const_iterator = array_iterator<mapped_type, true>;

  array() = delete;

  /**
   * @brief Array constructor
   *
   * @param comm Communicator to use for communication
   * @param size Global size to use to array
   */
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

  /**
   * @brief Array constructor taking default value
   *
   * @param comm Communicator to use for communication
   * @param size Global size to use for array
   * @param default_value Value to initialize all stored items with
   */
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

  /**
   * @brief Array constructor from std::initializer_list of values
   *
   * @param comm Communicator to use for communication
   * @param l Initializer list of values to put in array
   * @details Initializer list is assumed to be replicated on all ranks.
   * Initializer list only contains values to place in array. Indices assigned
   * to values are provided in sequential order. Array size is determined by
   * size of initializer list.
   */
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

  /**
   * @brief Array constructor from std::initializer_list of index-value pairs
   *
   * @param comm Communicator to use for communication
   * @param l Initializer list of index-value pairs to put in array
   * @details Initializer list is assumed to be replicated on all ranks.
   * Initializer list contains index-value pairs to place in array. Indices are
   * not assumed to be in sequential order or contiguous. Array size is
   * determined by max index within initializer list.
   */
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

  /**
   * @brief Construct array from existing YGM container
   *
   * @tparam T Existing container type
   * @param comm Communicator to use for communication
   * @param t YGM container containing values to put in array
   * @details Existing container contains only values. Indices are assigned
   * sequentially across ranks. Partitioning will likely not be the same between
   * existing container and constructed array.
   */
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
      this->async_insert(local_index++, value);
    });

    m_comm.barrier();
  }

  /**
   * @brief Construct array from existing YGM container of key-value pairs
   *
   * @tparam T Existing container type
   * @param comm Communicator to use for communication
   * @param t YGM container of key-value pairs to put in array.
   * @details Requires input container `for_all_args` to be a single item tuple
   * that is itself a key-value pair (e.g. works from a `ygm::container::bag`).
   * Array size is determined by finding the largest index across all ranks.
   */
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
      this->async_insert(std::get<0>(index_value), std::get<1>(index_value));
    });

    m_comm.barrier();
  }

  /**
   * @brief Construct array from existing YGM container of key-value pairs
   *
   * @tparam T Existing container type
   * @param comm Communicator to use for communication
   * @param t YGM container of key-value pairs to put in array
   * @details Requires input container's `for_all_args` to be a tuple containing
   * keys and values (e.g. works from a `ygm::container::map`). Array size is
   * determined by finding the largest index across all ranks.
   */
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
    t.for_all(
        [&max_index](const auto& index, [[maybe_unused]] const auto& value) {
          max_index = std::max<mapped_type>(index, max_index);
        });

    max_index = ::ygm::max(max_index, m_comm);

    resize(max_index + 1);

    t.for_all([this](const auto& index, const auto& value) {
      this->async_insert(index, value);
    });

    m_comm.barrier();
  }

  /**
   * @brief Construct array from existing STL container
   *
   * @tparam T Existing container type
   * @param comm Communicator to use for communication
   * @param t STL container containing values to put in array
   * @details Existing container contains only values. Values are assumed to be
   * distinct between ranks. Indices are assigned sequentially across ranks.
   * Partitioning will likely not be the same between existing container and
   * constructed array.
   */
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
                    this->async_insert(local_index++, value);
                  });

    m_comm.barrier();
  }

  /**
   * @brief Construct array from existing STL container of key-value pairs
   *
   * @tparam T Existing container type
   * @param comm Communicator to use for communication
   * @param t STL container of key-value pairs to put in array.
   * @details Requires existing container to have a `value_type` that contains
   * keys and values. Array size is determined by finding the largest index
   * across all ranks.
   */
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
      this->async_insert(std::get<0>(index_value), std::get<1>(index_value));
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

  /**
   * @brief Access to begin iterator of locally-held items
   *
   * @return Local iterator to beginning of items held by process.
   * @details Does not call `barrier()`.
   */
  iterator local_begin() {
    return iterator(this, partitioner.local_start(), 0);
  }

  /**
   * @brief Access to begin const_iterator of locally-held items for const array
   *
   * @return Local const iterator to beginning of items held by process.
   * @details Does not call `barrier()`.
   */
  const_iterator local_begin() const {
    return const_iterator(this, partitioner.local_start(), 0);
  }

  /**
   * @brief Access to begin const_iterator of locally-held items for const array
   *
   * @return Local const iterator to beginning of items held by process.
   * @details Does not call `barrier()`.
   */
  const_iterator local_cbegin() const {
    return const_iterator(const_cast<self_type*>(this),
                          partitioner.local_start(), 0);
  }

  /**
   * @brief Access to end iterator of locally-held items
   *
   * @return Local iterator to ending of items held by process.
   * @details Does not call `barrier()`.
   */
  iterator local_end() {
    return iterator(this, partitioner.local_start(), partitioner.local_size());
  }

  /**
   * @brief Access to end const_iterator of locally-held items for const array
   *
   * @return Local const iterator to ending of items held by process.
   * @details Does not call `barrier()`.
   */
  const_iterator local_end() const {
    return const_iterator(this, partitioner.local_start(),
                          partitioner.local_size());
  }

  /**
   * @brief Access to end const_iterator of locally-held items for const array
   *
   * @return Local const iterator to ending of items held by process.
   * @details Does not call `barrier()`.
   */
  const_iterator local_cend() const {
    return const_iterator(const_cast<self_type*>(this),
                          partitioner.local_start(), partitioner.local_size());
  }

  /**
   * @brief Insert a key and value into local storage.
   *
   * @param key Local index to store value at
   * @param value Vale to store
   * @details Assumes key (index) has already been converted to a local index.
   */
  void local_insert(const key_type& key, const mapped_type& value) {
    m_local_vec[partitioner.local_index(key)] = value;
  }

  /**
   * @brief Visit an item stored locally
   *
   * @tparam Function functor type
   * @tparam VisitorArgs... Variadic argument types
   * @param index Index to visit
   * @param fn User-provided function to execute at item
   * @param args... Arguments to pass to user functor
   */
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
      static_assert(
          ygm::detail::always_false<Function>,
          "remote array lambda must be "
          "invocable with (const "
          "key_type, mapped_type &, ...) or "
          "(ptr_type, const key_type, mapped_type &, ...) signatures");
    }
  }

  /**
   * @brief Set the value associated to given index
   *
   * @param index Index to store value at
   * @param value Value to store
   */
  void async_set(const key_type index, const mapped_type& value) {
    async_insert(index, value);
  }

  /**
   * @brief Apply a binary operation to a provided value and the value already
   * stored at a given index to update the stored value
   *
   * @tparam BinaryOp functor type
   * @param index Index to apply update at
   * @param value New value to update with
   * @param b Binary operation to apply
   */
  template <typename BinaryOp>
  void async_binary_op_update_value(const key_type                   index,
                                    const mapped_type&               value,
                                    [[maybe_unused]] const BinaryOp& b) {
    YGM_ASSERT_RELEASE(index < m_global_size);
    auto updater = []([[maybe_unused]] const key_type i, mapped_type& v,
                      const mapped_type& new_value) {
      BinaryOp binary_op;
      v = binary_op(v, new_value);
    };

    async_visit(index, updater, value);
  }

  /**
   * @brief Apply bitwise and to update stored value
   *
   * @param index Index to perform update at
   * @param value Value to "and" with current value
   */
  void async_bit_and(const key_type index, const mapped_type& value) {
    async_binary_op_update_value(index, value, std::bit_and<mapped_type>());
  }

  /**
   * @brief Apply bitwise or to update stored value
   *
   * @param index Index to perform update at
   * @param value Value to "or" with current value
   */
  void async_bit_or(const key_type index, const mapped_type& value) {
    async_binary_op_update_value(index, value, std::bit_or<mapped_type>());
  }

  /**
   * @brief Apply bitwise xor to update stored value
   *
   * @param index Index to perform update at
   * @param value Value to "xor" with current value
   */
  void async_bit_xor(const key_type index, const mapped_type& value) {
    async_binary_op_update_value(index, value, std::bit_xor<mapped_type>());
  }

  /**
   * @brief Apply logical and to update stored value
   *
   * @param index Index to perform update at
   * @param value Value to "and" with current value
   */
  void async_logical_and(const key_type index, const mapped_type& value) {
    async_binary_op_update_value(index, value, std::logical_and<mapped_type>());
  }

  /**
   * @brief Apply logical or to update stored value
   *
   * @param index Index to perform update at
   * @param value Value to "or" with current value
   */
  void async_logical_or(const key_type index, const mapped_type& value) {
    async_binary_op_update_value(index, value, std::logical_or<mapped_type>());
  }

  /**
   * @brief Apply multiplication to update stored value
   *
   * @param index Index to perform update at
   * @param value Value to multiply with current value
   */
  void async_multiplies(const key_type index, const mapped_type& value) {
    async_binary_op_update_value(index, value, std::multiplies<mapped_type>());
  }

  /**
   * @brief Apply division to update stored value
   *
   * @param index Index to perform update at
   * @param value Value to divide current value by
   */
  void async_divides(const key_type index, const mapped_type& value) {
    async_binary_op_update_value(index, value, std::divides<mapped_type>());
  }

  /**
   * @brief Apply addition to update stored value
   *
   * @param index Index to perform update at
   * @param value Value to add to current value
   */
  void async_plus(const key_type index, const mapped_type& value) {
    async_binary_op_update_value(index, value, std::plus<mapped_type>());
  }

  /**
   * @brief Apply subtraction to update stored value
   *
   * @param index Index to perform update at
   * @param value Value to subtract from current value
   */
  void async_minus(const key_type index, const mapped_type& value) {
    async_binary_op_update_value(index, value, std::minus<mapped_type>());
  }

  /**
   * @brief Apply a unary operation to the value already
   * stored at a given index to update the stored value
   *
   * @tparam UnaryOp functor type
   * @param index Index to apply update at
   * @param u Unary operation to apply
   */
  template <typename UnaryOp>
  void async_unary_op_update_value(const key_type                  index,
                                   [[maybe_unused]] const UnaryOp& u) {
    YGM_ASSERT_RELEASE(index < m_global_size);
    auto updater = []([[maybe_unused]] const key_type i, mapped_type& v) {
      UnaryOp u;
      v = u(v);
    };

    async_visit(index, updater);
  }

  /**
   * @brief Increment stored value
   *
   * @param index Index to perform update at
   */
  void async_increment(const key_type index) {
    async_unary_op_update_value(index,
                                [](const mapped_type& v) { return v + 1; });
  }

  /**
   * @brief Decrement stored value
   *
   * @param index Index to perform update at
   */
  void async_decrement(const key_type index) {
    async_unary_op_update_value(index,
                                [](const mapped_type& v) { return v - 1; });
  }

  const mapped_type& default_value() const;

  /**
   * @brief Set new global size for array
   *
   * @param size New global size
   * @param fill_value Value to initialize new values to (when expanding an
   * array)
   * @details This operation requires repartitioning the data already stored in
   * a container, which is a `O(old_size)` operation.
   */
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

  /**
   * @brief Set new global size for array with a default fill value
   *
   * @param size New global size
   * @details Equivalent to `resize(size, m_default_value)`
   */
  void resize(const size_type size) { resize(size, m_default_value); }

  /**
   * @brief Get the number of elements stored on the local process.
   *
   * @return Local size of array
   */
  size_t local_size() { return partitioner.local_size(); }

  /**
   * @brief Get the global size of the array
   *
   * @return Array's global size
   */
  size_t size() const {
    m_comm.barrier();
    return m_global_size;
  }

  /**
   * @brief Clear the local contents of the array and set size to 0
   *
   * @details Setting the local size to 0 cannot be performed independently of
   * other ranks. This operation needs to be called collectively for the array.
   */
  void local_clear() { resize(0); }

  /**
   * @brief Swap the local contents of an array.
   *
   * @param other The array to swap local contents with
   */
  void local_swap(self_type& other) {
    m_local_vec.swap(other.m_local_vec);
    std::swap(m_global_size, other.m_global_size);
    std::swap(m_default_value, other.m_default_value);
    std::swap(partitioner, other.partitioner);
  }

  /**
   * @brief Apply a lambda to all local elements
   *
   * @tparam Function functor type
   * @param fn Functor object to apply to all elements locally stored in the
   * array
   * @details This operation can be called non-collectively.
   */
  template <typename Function>
  void local_for_all(Function&& fn) {
    if constexpr (std::is_invocable<decltype(fn), const key_type,
                                    mapped_type&>()) {
      for (size_t i = 0; i < m_local_vec.size(); ++i) {
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

  /**
   * @brief Update a locally stored element by performing a binary operation
   * between it and a provided value
   *
   * @tparam ReductionOp functor type
   * @param index Global index to perform binary operation at. Must be found on
   * the local process.
   * @param value Value to combine with the currently-held value
   * @param reducer Binary operation to perform
   */
  template <typename ReductionOp>
  void local_reduce(const key_type index, const mapped_type& value,
                    ReductionOp reducer) {
    m_local_vec[partitioner.local_index(index)] =
        reducer(value, m_local_vec[partitioner.local_index(index)]);
  }

  /**
   * @brief Globally sort values in array in increasing order
   *
   * @details Partitions data using sampled pivots to approximately balance
   * values on ranks. Then use `std::sort` locally on values before reinserting
   * into the array.
   */
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

    YGM_ASSERT_RELEASE(pivots.size() == size_t(m_comm.size() - 1));

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

 private:
  ygm::comm&                       m_comm;
  typename ygm::ygm_ptr<self_type> pthis;
  size_type                        m_global_size;
  mapped_type                      m_default_value;
  std::vector<mapped_type>         m_local_vec;

 public:
  detail::block_partitioner<key_type> partitioner;
};

}  // namespace ygm::container
