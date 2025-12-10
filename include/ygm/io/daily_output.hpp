// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

// #include <chrono>
#include <ctime>
#include <filesystem>
#include <ygm/io/multi_output.hpp>

namespace ygm::io {

/**
 * @brief Class for writing output to a file for each day based on a timestamp
 * provided at the time of writing
 *
 * @tparam Partitioner Type used to assign filenames to ranks for writing
 */
template <typename Partitioner =
              ygm::container::detail::old_hash_partitioner<std::string>>
class daily_output {
 public:
  using self_type = daily_output<Partitioner>;

  /**
   * @brief Construct a daily_output object
   *
   * @param comm Communicator to use for communication
   * @param filename_prefix Prefix used when creating filenames
   * @param buffer_length Length of buffers to use before writing
   * @param append If false, existing files are overwritten. Otherwise, output
   * is appended to existing files.
   */
  daily_output(ygm::comm &comm, const std::string &filename_prefix,
               size_t buffer_length = 1024 * 1024, bool append = false)
      : m_multi_output(comm, filename_prefix, buffer_length, append),
        pthis(this) {}

  /**
   * @brief Write a line of output
   *
   * @tparam Args... Variadic types of output
   * @param timestamp Linux timestamp associated to use when assigning output to
   * a file
   * @param args... Variadic arguments to write to output file
   */
  template <typename... Args>
  void async_write_line(const uint64_t timestamp, Args &&...args) {
    // std::chrono::time_point<std::chrono::seconds> t(timestamp);
    std::time_t t(timestamp);

    std::tm *tm_ptr = std::gmtime(&t);

    const auto year{tm_ptr->tm_year + 1900};
    const auto month{tm_ptr->tm_mon + 1};
    const auto day{tm_ptr->tm_mday};

    std::string date_path{std::to_string(year) + "/" + std::to_string(month) +
                          "/" + std::to_string(day)};

    m_multi_output.async_write_line(date_path, std::forward<Args>(args)...);
  }

  /**
   * @brief Access to own ygm_ptr
   *
   * @return `ygm_ptr` used by the container when identifying itself in `async`
   * calls on the `ygm::comm`
   */
  typename ygm::ygm_ptr<self_type> get_ygm_ptr() {
    return static_cast<self_type *>(this)->pthis;
  }

  /**
   * @brief Const access to own ygm ptr
   *
   * @return `ygm_ptr` to const version of container
   */
  const typename ygm::ygm_ptr<self_type> get_ygm_ptr() const {
    return static_cast<const self_type *>(this)->pthis;
  }

 private:
  multi_output<Partitioner>        m_multi_output;
  typename ygm::ygm_ptr<self_type> pthis;
};
}  // namespace ygm::io
