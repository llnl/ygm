// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include "spdlog/pattern_formatter.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/spdlog.h"

#include <filesystem>
#include <iostream>

namespace ygm {

enum class log_level {
  off      = 0,
  critical = 1,
  error    = 2,
  warn     = 3,
  info     = 4,
  debug    = 5
};

enum class logger_target { file, stdout, stderr };

namespace detail {

static std::vector<spdlog::level::level_enum> ygm_level_to_spdlog_level{
    spdlog::level::level_enum::off,  spdlog::level::level_enum::critical,
    spdlog::level::level_enum::err,  spdlog::level::level_enum::warn,
    spdlog::level::level_enum::info, spdlog::level::level_enum::debug};

class rank_formatter_flag : public spdlog::custom_flag_formatter {
 public:
  rank_formatter_flag(const int rank) : m_rank(rank) {
    m_rank_msg = std::string("Rank ") + std::to_string(m_rank);
  }

  void format(const spdlog::details::log_msg &, const std::tm &,
              spdlog::memory_buf_t &dest) override {
    dest.append(m_rank_msg.data(), m_rank_msg.data() + m_rank_msg.size());
  }

  std::unique_ptr<custom_flag_formatter> clone() const override {
    return spdlog::details::make_unique<rank_formatter_flag>(m_rank);
  }

 private:
  int         m_rank;
  std::string m_rank_msg;
};

/**
 * @brief Simple logger for applications using YGM
 */
class logger {
 public:
  using rank_logger_t    = spdlog::logger;
  using rank_file_sink_t = spdlog::sinks::basic_file_sink_st;
  using rank_cout_sink_t = spdlog::sinks::stdout_sink_st;
  using rank_cerr_sink_t = spdlog::sinks::stderr_sink_st;

  logger(const int rank) : logger(rank, std::filesystem::path("./log/")) {}

  logger(const int rank, const std::filesystem::path &path)
      : m_logger_target(logger_target::file),
        m_cout_logger("ygm_cout_logger", std::make_shared<rank_cout_sink_t>()),
        m_cerr_logger("ygm_cerr_logger", std::make_shared<rank_cerr_sink_t>()),
        m_path(path) {
    if (std::filesystem::is_directory(path)) {
      m_path += "/ygm_logs";
    }

    // Set custom logging message format for stdout and stderr to include MPI
    // rank
    auto stdout_formatter = std::make_unique<spdlog::pattern_formatter>();
    stdout_formatter->add_flag<rank_formatter_flag>('k', rank).set_pattern(
        "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [%k] %v");
    m_cout_logger.set_formatter(std::move(stdout_formatter));

    auto stderr_formatter = std::make_unique<spdlog::pattern_formatter>();
    stderr_formatter->add_flag<rank_formatter_flag>('k', rank).set_pattern(
        "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [%k] %v");
    m_cerr_logger.set_formatter(std::move(stderr_formatter));

    // We will control logging levels
    m_cout_logger.set_level(spdlog::level::trace);
    m_cerr_logger.set_level(spdlog::level::trace);
  }

  void set_path(const std::filesystem::path p) {
    m_path = p;

    if (m_file_logger_ptr) {
      m_file_logger_ptr.reset();
    }
  }

  std::filesystem::path get_path() { return m_path; }

  void set_log_level(const log_level level) { m_log_level = level; }

  log_level get_log_level() const { return m_log_level; }

  void set_logger_target(const logger_target target) {
    m_logger_target = target;
  }

  logger_target get_logger_target() const { return m_logger_target; }

  template <typename... Args>
  void log(const std::vector<logger_target> &targets, const log_level level,
           Args &&...args) const {
    for (const auto target : targets) {
      log_impl(target, level, std::forward<Args>(args)...);
    }
  }

  template <typename... Args>
  void log(const log_level level, Args &&...args) const {
    log(std::vector({m_logger_target}), level, std::forward<Args>(args)...);
  }

  /*
   * @brief Force a flush of file-backed logs
   */
  void flush() {
    if (m_file_logger_ptr) {
      m_file_logger_ptr->flush();
    }
  }

 private:
  template <typename... Args>
  void log_impl(logger_target t, const log_level level, Args &&...args) const {
    if (level > m_log_level) {
      return;
    }

    switch (t) {
      case logger_target::file:
        if (not m_file_logger_ptr) {
          std::filesystem::create_directories(m_path.parent_path());

          m_file_logger_ptr = std::make_unique<spdlog::logger>(
              "ygm_file_logger",
              std::make_shared<rank_file_sink_t>(m_path.c_str(), false));
          m_file_logger_ptr->set_level(spdlog::level::trace);
        }
        m_file_logger_ptr->log(
            ygm_level_to_spdlog_level[static_cast<size_t>(level)], args...);
        break;

      case logger_target::stdout:
        m_cout_logger.log(ygm_level_to_spdlog_level[static_cast<size_t>(level)],
                          args...);
        break;

      case logger_target::stderr:
        m_cerr_logger.log(ygm_level_to_spdlog_level[static_cast<size_t>(level)],
                          args...);
        break;
    }
  }

  logger_target                          m_logger_target;
  mutable std::unique_ptr<rank_logger_t> m_file_logger_ptr;
  mutable rank_logger_t                  m_cout_logger;
  mutable rank_logger_t                  m_cerr_logger;

  log_level m_log_level = log_level::off;

  std::filesystem::path m_path;
};

}  // namespace detail
}  // namespace ygm
