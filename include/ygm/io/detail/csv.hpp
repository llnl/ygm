// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <iomanip>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace ygm::io::detail {

class csv_field {
 public:
  csv_field(const std::string &f) : m_f(f) {}

  bool is_integer() const {
    int64_t            test;
    std::istringstream ss{m_f};
    ss >> test;
    return !ss.fail() && ss.eof();
  }

  int64_t as_integer() const {
    int64_t            to_return;
    std::istringstream ss{m_f};
    ss >> to_return;
    assert(!ss.fail() && ss.eof());
    return to_return;
  }

  bool is_unsigned_integer() const {
    uint64_t           test;
    std::istringstream ss{m_f};
    ss >> test;
    return !ss.fail() && ss.eof();
  }

  uint64_t as_unsigned_integer() const {
    uint64_t           to_return;
    std::istringstream ss{m_f};
    ss >> to_return;
    assert(!ss.fail() && ss.eof());
    return to_return;
  }

  bool is_double() const {
    double             test;
    std::istringstream ss{m_f};
    ss >> test;
    return !ss.fail() && ss.eof();
  }

  double as_double() const {
    double             to_return;
    std::istringstream ss{m_f};
    ss >> to_return;
    assert(!ss.fail() && ss.eof());
    return to_return;
  }

  const std::string &as_string() const { return m_f; }

 private:
  friend std::ostream &operator<<(std::ostream &os, const csv_field &f);
  std::string          m_f;
};

class csv_line {
 public:
  using vector_type            = std::vector<csv_field>;
  using size_type              = vector_type::size_type;
  using reference              = vector_type::reference;
  using const_reference        = vector_type::const_reference;
  using iterator               = vector_type::iterator;
  using const_iterator         = vector_type::const_iterator;
  using reverse_iterator       = vector_type::reverse_iterator;
  using const_reverse_iterator = vector_type::const_reverse_iterator;

  csv_line(const std::map<std::string, int> &header_map)
      : m_header_map_ref(header_map) {};

  void push_back(const csv_field &f) { m_csv_fields.push_back(f); }

  size_type size() const { return m_csv_fields.size(); }

  reference operator[](size_type n) { return m_csv_fields[n]; }

  const_reference operator[](size_type n) const { return m_csv_fields[n]; }

  const_reference operator[](const std::string &key) const {
    return m_csv_fields[m_header_map_ref.at(key)];
  }

  iterator               begin() { return m_csv_fields.begin(); }
  iterator               end() { return m_csv_fields.end(); }
  const_iterator         begin() const { return m_csv_fields.begin(); }
  const_iterator         end() const { return m_csv_fields.end(); }
  reverse_iterator       rbegin() { return m_csv_fields.rbegin(); }
  reverse_iterator       rend() { return m_csv_fields.rend(); }
  const_reverse_iterator rbegin() const { return m_csv_fields.rbegin(); }
  const_reverse_iterator rend() const { return m_csv_fields.rend(); }
  const_iterator         cbegin() const { return m_csv_fields.cbegin(); }
  const_iterator         cend() const { return m_csv_fields.cend(); }
  const_reverse_iterator crbegin() const { return m_csv_fields.crbegin(); }
  const_reverse_iterator crend() const { return m_csv_fields.crend(); }

 private:
  std::vector<csv_field>            m_csv_fields;
  const std::map<std::string, int> &m_header_map_ref;
};

inline std::ostream &operator<<(std::ostream &os, const csv_field &f) {
  return os << f.as_string();
}

inline csv_line parse_csv_line(
    const std::string line, const std::map<std::string, int> &header_map_ref) {
  csv_line line_fields(header_map_ref);
  if (line.empty() || line[0] == '#') {
    return line_fields;
  }
  std::stringstream ssline{line};
  while (ssline >> std::ws) {
    std::string csv_field;
    if (ssline.peek() == '"') {
      ssline >> std::quoted(csv_field);
      if (ssline) {
        line_fields.push_back(csv_field);
      }
      ssline.ignore(256, ',');
    } else {
      std::getline(ssline, csv_field, ',');
      if (ssline) {
        line_fields.push_back(csv_field);
      }
    }
  }
  return line_fields;
}

inline std::map<std::string, int> parse_csv_headers(
    const std::string header_line) {
  std::map<std::string, int> header_map;

  std::stringstream ssline(header_line);
  int               column_num{0};
  while (ssline >> std::ws) {
    std::string header_field;
    if (ssline.peek() == '"') {
      ssline >> std::quoted(header_field);
      if (ssline) {
        header_map[header_field] = column_num++;
      }
      ssline.ignore(256, ',');
    } else {
      std::getline(ssline, header_field, ',');
      if (ssline) {
        header_map[header_field] = column_num++;
      }
    }
  }

  return header_map;
}

inline std::string convert_type_string(
    const std::vector<csv_field> &line_fields) {
  std::stringstream ss;
  for (const auto &f : line_fields) {
    if (f.is_integer()) {
      ss << "I";
    } else if (f.is_double()) {
      ss << "D";
    } else {
      ss << "S";
    }
  }
  return ss.str();
}

}  // namespace ygm::io::detail
