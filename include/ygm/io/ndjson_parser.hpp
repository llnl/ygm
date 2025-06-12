// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#if !__has_include(<boost/json/src.hpp>)
#error BOOST >= 1.75 is required for Boost.JSON
#endif

#include <ygm/comm.hpp>
#include <ygm/container/detail/base_iteration.hpp>
#include <ygm/detail/cereal_boost_json.hpp>
#include <ygm/io/line_parser.hpp>

namespace ygm::io {
std::size_t json_erase(boost::json::object            &obj,
                       const std::vector<std::string> &keys) {
  std::size_t num_erased = 0;
  for (const auto &key : keys) {
    num_erased += obj.erase(key);
  }
  return num_erased;
}

std::size_t json_filter(boost::json::object            &obj,
                        const std::vector<std::string> &include_keys) {
  std::set<std::string>    include_keys_set{include_keys.begin(),
                                         include_keys.end()};
  std::vector<std::string> keys_to_erase;
  for (auto itr = obj.begin(), end = obj.end(); itr != end; ++itr) {
    if (include_keys_set.count(itr->key().data()) == 0) {
      keys_to_erase.emplace_back(itr->key().data());
    }
  }
  return json_erase(obj, keys_to_erase);
}

class ndjson_parser : public ygm::container::detail::base_iteration_value<
                          ndjson_parser, std::tuple<boost::json::object>> {
 public:
  using for_all_args = std::tuple<boost::json::object>;
  template <typename... Args>
  ndjson_parser(Args &&...args) : m_lp(std::forward<Args>(args)...) {}

  /**
   * @brief Executes a user function for every CSV record in a set of files.
   *
   * @tparam Function
   * @param fn User function to execute
   */
  template <typename Function>
  void for_all(Function fn) {
    m_lp.for_all([fn](const std::string &line) {
      try {
        fn(boost::json::parse(line).as_object());
      } catch (...) {
        ++m_num_invalid_records;
      }
    });
  }

  ygm::comm &comm() { return m_lp.comm(); }

  const ygm::comm &comm() const { return m_lp.comm(); }

  size_t num_invalid_records() {
    return ygm::sum(m_num_invalid_records, m_lp.comm());
  }

 private:
  line_parser m_lp;

  size_t m_num_invalid_records{0};
};

}  // namespace ygm::io
