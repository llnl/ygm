// Copyright 2019-2021 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <ygm/comm.hpp>

#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"

#include <filesystem>

namespace ygm {

class comm;

namespace detail {

/**
 * @brief Simple logger for applications using YGM
 */
class logger {
 public:
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

  template <typename... Args>
  void log(Args &&...args) const {
    if (not m_logger_ptr) {
      std::filesystem::create_directories(m_path.parent_path());

      m_logger_ptr =
          spdlog::basic_logger_mt("ygm_logger", m_path.c_str(), true);
    }
    m_logger_ptr->info(args...);
  }

 private:
  mutable std::shared_ptr<spdlog::logger> m_logger_ptr;

  std::filesystem::path m_path;
};

}  // namespace detail
}  // namespace ygm
