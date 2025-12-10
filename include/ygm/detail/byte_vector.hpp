#pragma once

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

namespace ygm::detail {
class byte_vector {
  using value_type      = std::byte;
  using pointer         = std::byte*;
  using reference       = std::byte&;
  using const_reference = const std::byte&;

 public:
  class Byte_Iterator {
   public:
    Byte_Iterator() : m_bv(nullptr), i(0) {}
    Byte_Iterator(byte_vector* v, size_t i) : m_bv(v), i(i) {}

    reference       operator*() { return (*m_bv)[i]; }
    const_reference operator*() const { return (*m_bv)[i]; }
    reference       operator[](int n) { return (*m_bv)[i + n]; }
    const_reference operator[](int n) const { return (*m_bv)[i + n]; }

    Byte_Iterator& operator++() {
      ++i;
      return *this;
    }
    Byte_Iterator& operator--() {
      --i;
      return *this;
    }
    Byte_Iterator operator++(int) {
      Byte_Iterator r(*this);
      ++i;
      return r;
    }
    Byte_Iterator operator--(int) {
      Byte_Iterator r(*this);
      --i;
      return r;
    }

    Byte_Iterator& operator+=(int n) {
      i += n;
      return *this;
    }
    Byte_Iterator& operator-=(int n) {
      i -= n;
      return *this;
    }

    bool operator<(Byte_Iterator const& r) const { return i < r.i; }
    bool operator<=(Byte_Iterator const& r) const { return i <= r.i; }
    bool operator>(Byte_Iterator const& r) const { return i > r.i; }
    bool operator>=(Byte_Iterator const& r) const { return i >= r.i; }
    bool operator!=(Byte_Iterator const& r) const { return i != r.i; }
    bool operator==(Byte_Iterator const& r) const { return i == r.i; }

   private:
    byte_vector* m_bv;
    size_t       i;
  };

  byte_vector() : m_data(nullptr), m_size(0), m_capacity(0) {}

  byte_vector(size_t set_capacity) : m_size(0) {
    m_capacity = get_page_aligned_size(set_capacity);
    // now that the shm_file is the correct size we can memory map to it.
    m_data = (pointer)mmap(NULL, m_capacity, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    if (m_data == MAP_FAILED) {
      std::cerr << strerror(errno) << std::endl;
      throw std::runtime_error("mmap failed to allocate byte_vector:" +
                               std::string(strerror(errno)));
    }
  }

  ~byte_vector() { munmap(m_data, m_capacity); }

  byte_vector(byte_vector&)       = default;
  byte_vector(const byte_vector&) = default;
  byte_vector(byte_vector&&)      = default;

  const_reference operator[](int i) const { return m_data[i]; }
  reference       operator[](int i) { return m_data[i]; }

  pointer data() const { return m_data; }
  bool    empty() const { return m_size == 0; }
  void    clear() { m_size = 0; }
  size_t  size() const { return m_size; }
  size_t  capacity() const { return m_capacity; }

  Byte_Iterator begin() { return Byte_Iterator(this, 0); }
  Byte_Iterator end() { return Byte_Iterator(this, m_size); }

  void swap(byte_vector& other) {
    std::swap(m_data, other.m_data);
    std::swap(m_size, other.m_size);
    std::swap(m_capacity, other.m_capacity);
  }

  /**
   * @brief Manually reseerves memory for the byte_vector.
   * @param cap The new capacity to reserve, it will be page aligned.
   */
  void reserve(size_t cap) {
    size_t new_capacity = get_page_aligned_size(cap);
    if (m_data == nullptr) {
      m_capacity = new_capacity;
      // now that the shm_file is the correct size we can memory map to it.
      m_data = (pointer)mmap(NULL, m_capacity, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
      if (m_data == MAP_FAILED) {
        throw std::runtime_error(
            "mmap failed to initialize empty byte_vector:" +
            std::string(strerror(errno)));
      }
      return;
    }
// if max osx handler
#if __APPLE__ || YGM_DONT_USE_MREMAP
    pointer temp = (pointer)mmap(NULL, new_capacity, PROT_READ | PROT_WRITE,
                                 MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    if (temp == MAP_FAILED) {
      throw std::runtime_error("mmap failed" + std::string(strerror(errno)));
    }
    memcpy(temp, m_data, m_size);
    munmap(m_data, m_capacity);
    m_data = temp;
#else
    try {
      pointer new_data =
          (pointer)mremap(m_data, m_capacity, new_capacity, MREMAP_MAYMOVE);
      if (new_data == MAP_FAILED) {
        throw std::runtime_error("mremap failed to resize byte_vector:" +
                                 std::string(strerror(errno)));
      } else {
        m_data = new_data;
      }
    } catch (
        const std::runtime_error& err) {  // Fallback to mmap if mremap fails
      pointer temp = (pointer)mmap(NULL, new_capacity, PROT_READ | PROT_WRITE,
                                   MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
      if (temp == MAP_FAILED) {
        throw std::runtime_error("mmap failed" + std::string(strerror(errno)));
      }
      memcpy(temp, m_data, m_size);
      munmap(m_data, m_capacity);
      m_data = temp;
    }
#endif
    m_capacity = new_capacity;
  }

  // @brief Resizes the byte_vector to the new size if the new size is greater
  // than the current capacity
  void resize(size_t s) {
    if (s > m_capacity) this->reserve(s);
    m_size = s;
  }

  /**
   * @brief Appends bytes to the byte_vector.
   *
   * @param d Pointer to the data to be appended.
   * @param s Number of bytes to append.
   */
  void push_bytes(const void* d, size_t s) {
    if (s > m_capacity - m_size) {
      size_t new_capacity = std::max(m_capacity * 2, m_size + s);
      this->reserve(new_capacity);
    }
    memcpy(m_data + m_size, d, s);
    m_size += s;
  }

  void push_bytes(void* d, size_t s) { push_bytes((const void*)d, s); }

 private:
  /**
   * @brief Returns the page aligned size for a given number of bytes
   */
  size_t get_page_aligned_size(size_t s) const {
    auto pagesize  = getpagesize();
    auto num_pages = s / pagesize;
    if (s % pagesize != 0) num_pages++;
    return num_pages * pagesize;
  }

  pointer m_data;
  size_t  m_size;
  size_t  m_capacity;
};

}  // namespace ygm::detail
