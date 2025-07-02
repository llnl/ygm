// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

/**
 * @file
 * @brief Support for boost::static<string>
 */

#include <boost/static_string/static_string.hpp>
#include <cereal/cereal.hpp>

namespace cereal {
/// \brief Save function for boost::json::string,
/// which is different std::string or boost::string.
template <class Archive, std::size_t N>
void CEREAL_SAVE_FUNCTION_NAME(Archive                       &archive,
                               const boost::static_string<N> &str) {
  // Length (#of chars in the string)
  archive(cereal::make_size_tag(static_cast<std::size_t>(str.size())));

  // String data
  archive(cereal::binary_data(str.data(), str.size() * sizeof(char)));
}

/// \brief Load function for boost::json::string,
/// which is different std::string or boost::string.
template <class Archive, std::size_t N>
void CEREAL_LOAD_FUNCTION_NAME(Archive &archive, boost::static_string<N> &str) {
  std::size_t size;
  archive(cereal::make_size_tag(size));

  str.resize(size);
  archive(
      cereal::binary_data(const_cast<char *>(str.data()), size * sizeof(char)));
}

}  // namespace cereal