// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#if !__has_include(<boost/json/src.hpp>)
#error BOOST >= 1.75 is required for Boost.JSON
#endif

#include <boost/json.hpp>
#include <cereal/cereal.hpp>
#include <string>

#include <ygm/detail/ygm_cereal_archive.hpp>

namespace cereal {

namespace {
namespace bj = ::boost::json;
}

/// \brief Boost JSON value kind
enum class bjson_value_kind : int8_t {
  null,
  bool_,
  int64,
  uint64,
  double_,
  string,
  array,
  object
};

/// \brief Save function for boost::json::value.
template <class Archive>
inline void CEREAL_SAVE_FUNCTION_NAME(Archive         &archive,
                                      const bj::value &value) {
  if (value.is_null()) {
    archive(bjson_value_kind::null);
    // Save nothing for null
  } else if (value.is_bool()) {
    archive(bjson_value_kind::bool_);
    archive(value.as_bool());
  } else if (value.is_int64()) {
    archive(bjson_value_kind::int64);
    archive(value.as_int64());
  } else if (value.is_uint64()) {
    archive(bjson_value_kind::uint64);
    archive(value.as_uint64());
  } else if (value.is_double()) {
    archive(bjson_value_kind::double_);
    archive(value.as_double());
  } else if (value.is_string()) {
    archive(bjson_value_kind::string);
    archive(value.as_string());
  } else if (value.is_array()) {
    archive(bjson_value_kind::array);
    archive(value.as_array());
  } else if (value.is_object()) {
    archive(bjson_value_kind::object);
    archive(value.as_object());
  }
}

/// \brief Load function for boost::json::value.
template <class Archive>
inline void CEREAL_LOAD_FUNCTION_NAME(Archive &archive, bj::value &value) {
  bjson_value_kind kind;
  archive(kind);

  if (kind == bjson_value_kind::null) {
    value.emplace_null();
  } else if (kind == bjson_value_kind::bool_) {
    archive(value.emplace_bool());
  } else if (kind == bjson_value_kind::int64) {
    archive(value.emplace_int64());
  } else if (kind == bjson_value_kind::uint64) {
    archive(value.emplace_uint64());
  } else if (kind == bjson_value_kind::double_) {
    archive(value.emplace_double());
  } else if (kind == bjson_value_kind::string) {
    archive(value.emplace_string());
  } else if (kind == bjson_value_kind::array) {
    archive(value.emplace_array());
  } else if (kind == bjson_value_kind::object) {
    archive(value.emplace_object());
  }
}

/// \brief Save function for boost::json::object.
template <class Archive>
inline void CEREAL_SAVE_FUNCTION_NAME(Archive          &archive,
                                      const bj::object &object) {
  archive(make_size_tag(static_cast<std::size_t>(object.size())));

  for (const auto &pair : object) {
    // pair.key() returns boost::json::string_view, which is std::string_view.
    // The concept of string_view does not work well with cereal.
    // Thus, we save keys manually using binary_data.
    const auto &key = pair.key();
    archive(cereal::make_size_tag(static_cast<std::size_t>(key.size())));
    archive(cereal::binary_data(key.data(), key.size() * sizeof(char)));

    archive(pair.value());
  }
}

/// \brief Load function for boost::json::object.
template <class Archive>
inline void CEREAL_LOAD_FUNCTION_NAME(Archive &archive, bj::object &object) {
  std::size_t num_objects;
  archive(make_size_tag(num_objects));

  std::string key;  // reuse buffer to speed up
  for (std::size_t i = 0; i < num_objects; ++i) {
    std::size_t key_length;
    archive(make_size_tag(key_length));
    key.resize(key_length);
    archive(cereal::binary_data(const_cast<char *>(key.data()),
                                key_length * sizeof(char)));

    bj::value value;
    archive(value);

    object.emplace(key, std::move(value));
  }
}

/// \brief Save function for boost::json::array.
template <class Archive>
inline void CEREAL_SAVE_FUNCTION_NAME(Archive         &archive,
                                      const bj::array &array) {
  archive(make_size_tag(static_cast<std::size_t>(array.size())));

  for (const auto &item : array) {
    archive(item);
  }
}

/// \brief Load function for boost::json::array.
template <class Archive>
inline void CEREAL_LOAD_FUNCTION_NAME(Archive &archive, bj::array &array) {
  std::size_t size;
  archive(make_size_tag(size));
  array.reserve(size);

  for (std::size_t i = 0; i < size; ++i) {
    bj::value value;
    archive(value);
    array.push_back(std::move(value));
  }
}

/// \brief Save function for boost::json::string,
/// which is different std::string or boost::string.
template <class Archive>
void CEREAL_SAVE_FUNCTION_NAME(Archive &archive, const bj::string &str) {
  // Length (#of chars in the string)
  archive(cereal::make_size_tag(static_cast<std::size_t>(str.size())));

  // String data
  archive(cereal::binary_data(str.data(), str.size() * sizeof(char)));
}

/// \brief Load function for boost::json::string,
/// which is different std::string or boost::string.
template <class Archive>
void CEREAL_LOAD_FUNCTION_NAME(Archive &archive, bj::string &str) {
  std::size_t size;
  archive(cereal::make_size_tag(size));

  str.resize(size);
  archive(
      cereal::binary_data(const_cast<char *>(str.data()), size * sizeof(char)));
}

}  // namespace cereal
