// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include "spdlog/sinks/basic_file_sink.h"

#include <filesystem>

namespace ygm {

enum class log_level {
  off      = 0,
  critical = 1,
  error    = 2,
  warn     = 3,
  info     = 4,
  debug    = 5
};

namespace detail {

/**
 * @brief Simple logger for applications using YGM
 */
class logger {
 public:
  using rank_logger_t    = spdlog::logger;
  using rank_file_sink_t = spdlog::sinks::basic_file_sink_st;

  logger() : logger(std::filesystem::path("./log/")) {}

  logger(const std::filesystem::path &path) : m_path(path) {
    if (std::filesystem::is_directory(path)) {
      m_path += "/ygm_logs";
    }
  }

  void set_path(const std::filesystem::path p) {
    m_path = p;

    if (m_logger_ptr) {
      m_logger_ptr.reset();
    }
  }

  std::filesystem::path get_path() { return m_path; }

  void set_log_level(const log_level level) { m_log_level = level; }

  log_level get_log_level() { return m_log_level; }

  template <typename... Args>
  void log(const log_level level, Args &&...args) const {
    if (level > m_log_level) {
      return;
    }

    if (not m_logger_ptr) {
      std::filesystem::create_directories(m_path.parent_path());

      m_logger_ptr = std::make_unique<spdlog::logger>(
          "ygm_logger",
          std::make_shared<rank_file_sink_t>(m_path.c_str(), false));
    }
    m_logger_ptr->info(args...);
  }

 private:
  mutable std::unique_ptr<rank_logger_t> m_logger_ptr;

  log_level m_log_level = log_level::off;

  std::filesystem::path m_path;
};

}  // namespace detail
}  // namespace ygm
