// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <fstream>
#include <map>
#include <string>
#include <vector>
#include <ygm/container/detail/base_iteration.hpp>
#include <ygm/io/detail/csv.hpp>
#include <ygm/io/line_parser.hpp>

namespace ygm::io {

/**
 * @brief Class for parsing collections of CSV files in distributed memory
 */
class csv_parser : public ygm::container::detail::base_iteration_value<
                       csv_parser, std::tuple<std::vector<detail::csv_field>>> {
 public:
  using for_all_args = std::tuple<std::vector<detail::csv_field>>;

  template <typename... Args>
  csv_parser(Args&&... args)
      : m_lp(std::forward<Args>(args)...), m_has_headers(false) {}

  /**
   * @brief Executes a user function for every CSV record in a set of files.
   *
   * @tparam Function functor type
   * @param fn User function to execute
   */
  template <typename Function>
  void for_all(Function fn) {
    using namespace ygm::io::detail;

    auto handle_line_lambda = [fn, this](const std::string& line) {
      auto vfields = parse_csv_line(line, m_header_map);
      // auto stypes    = convert_type_string(vfields);
      // todo, detect if types are inconsistent between records
      if (vfields.size() > 0) {
        fn(vfields);
      }
    };

    m_lp.for_all(handle_line_lambda);
  }

  /**
   * @brief Read the header of a CSV file
   */
  void read_headers() {
    using namespace ygm::io::detail;
    auto header_line = m_lp.read_first_line();
    m_lp.set_skip_first_line(true);
    m_header_map  = parse_csv_headers(header_line);
    m_has_headers = true;
  }

  /**
   * @brief Checks for existence of a column label within headers
   *
   * @param label Header label to search for within headers
   */
  bool has_header(const std::string& label) {
    return m_has_headers && (m_header_map.find(label) != m_header_map.end());
  }

  ygm::comm& comm() { return m_lp.comm(); }

  const ygm::comm& comm() const { return m_lp.comm(); }

 private:
  line_parser m_lp;

  std::map<std::string, int> m_header_map;
  bool                       m_has_headers;
};  // namespace ygm::io
}  // namespace ygm::io
