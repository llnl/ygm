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

template <typename Partitioner =
              ygm::container::detail::old_hash_partitioner<std::string>>
class daily_output {
 public:
  using self_type = daily_output<Partitioner>;

  daily_output(ygm::comm &comm, const std::string &filename_prefix,
               size_t buffer_length = 1024 * 1024, bool append = false)
      : m_multi_output(comm, filename_prefix, buffer_length, append) {}

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

 private:
  multi_output<Partitioner> m_multi_output;
};
}  // namespace ygm::io
