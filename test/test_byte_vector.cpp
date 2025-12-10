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

int main() {
  std::vector<std::string> vec_sentences = {
      "Four score and seven years ago",
      "our fathers brought forth on this continent",
      "a new nation conceived in liberty"};

  ygm::detail::byte_vector buffer;
  {
    for (const auto& s : vec_sentences) {
      buffer.push_bytes(s.data(), s.size());
    }
  }

  {
    std::vector<char> output(buffer.size());
    memcpy((void*)output.data(), (void*)buffer.data(), buffer.size());
    auto str_it = output.begin();
    auto bv_it  = buffer.begin();
    for (const auto& s : vec_sentences) {
      for (size_t i = 0; i < s.size(); i++) {
        YGM_ASSERT_RELEASE(s[i] == (char)(*bv_it) && s[i] == (*str_it));
        bv_it++;
        str_it++;
      }
    }
  }

  {
    // iterator operator tests
    auto test_it1 = buffer.begin();
    auto test_it2 = buffer.begin();
    // Testing logical Operators
    YGM_ASSERT_RELEASE(test_it1 == test_it2);
    YGM_ASSERT_RELEASE(test_it1 <= test_it2);
    YGM_ASSERT_RELEASE(test_it1 >= test_it2);
    test_it2++;
    YGM_ASSERT_RELEASE(test_it1 != test_it2);
    YGM_ASSERT_RELEASE(test_it1 < test_it2);
    YGM_ASSERT_RELEASE(test_it1 <= test_it2);
    YGM_ASSERT_RELEASE(test_it2 > test_it1);
    YGM_ASSERT_RELEASE(test_it2 >= test_it1);
    test_it2--;
    YGM_ASSERT_RELEASE(test_it1 == test_it2);
    test_it2 += 3;                                 // testing +=
    YGM_ASSERT_RELEASE(test_it1[3] == *test_it2);  // testing [] operator
    test_it2 -= 3;                                 // testing -=
    YGM_ASSERT_RELEASE(test_it1 == test_it2);

    // testing postfix and prefix operators
    YGM_ASSERT_RELEASE(test_it1 == test_it2++);
    YGM_ASSERT_RELEASE(test_it1 == --test_it2);

    YGM_ASSERT_RELEASE(!(test_it1 < test_it2++));
    YGM_ASSERT_RELEASE(!(test_it1 > --test_it2));

    test_it2++;  // test_it1 is at 0, test_it2 is at 1
    YGM_ASSERT_RELEASE(test_it1 != test_it2--);
    YGM_ASSERT_RELEASE(test_it1 == test_it2);
    YGM_ASSERT_RELEASE(test_it1 < ++test_it2);
  }

  return 0;
}
