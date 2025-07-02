// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <ygm/detail/byte_vector.hpp>
#include <ygm/detail/ygm_cereal_archive.hpp>
#include <ygm/utility/assert.hpp>
#include <ygm/utility/boost_json.hpp>

namespace bj = boost::json;

std::string json_string = R"(
      {
        "pi": 3.141,
        "happy": true,
        "name": "Alice",
        "nothing": null,
        "list": [1, 0, 2],
        "object": {
          "currency": "USD",
          "value": -10
        }
      }
)";

int main() {
  ygm::detail::byte_vector buffer;
  {
    const bj::value          value = bj::parse(json_string);
    cereal::YGMOutputArchive archive(buffer);
    archive(value);
  }

  {
    cereal::YGMInputArchive archive(buffer.data(), buffer.size());
    bj::value               load_value;
    archive(load_value);

    const bj::value original_value = bj::parse(json_string);
    // std::cout << original_value << std::endl;
    // std::cout << load_value << std::endl;
    YGM_ASSERT_RELEASE(original_value == load_value);
  }

  return 0;
}