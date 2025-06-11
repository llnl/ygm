// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#include <functional>
#include <string>
#include <ygm/comm.hpp>
#include <ygm/container/bag.hpp>
#include <ygm/container/map.hpp>
#include <ygm/io/line_parser.hpp>

int main(int argc, char **argv) {
  ygm::comm world(&argc, &argv);

  // gm::container::bag<std::string> bbag(world);
  //  if (world.rank0()) {
  //    bbag.async_insert("dog");
  //    bbag.async_insert("apple");
  //    bbag.async_insert("red");
  //  } else if (world.rank() == 1) {
  //    bbag.async_insert("cat");
  //    bbag.async_insert("banana");
  //    bbag.async_insert("blue");
  //  } else if (world.rank() == 2) {
  //    bbag.async_insert("fish");
  //    bbag.async_insert("pear");
  //    bbag.async_insert("green");
  //  } else if (world.rank() == 3) {
  //    bbag.async_insert("snake");
  //    bbag.async_insert("cherry");
  //    bbag.async_insert("yellow");
  //  }

  // world.barrier();

  // world.cout0("BEFORE FILTER");
  // world.barrier();

  // bbag.for_all([&world](std::string s) { world.cout(s); });
  // world.barrier();

  // world.cout0("WITH FILTER");
  // world.barrier();

  // bbag.filter([](std::string s) { return s.size() == 3; })
  //     .for_all([&world](std::string s) { world.cout(s); });

  // world.barrier();

  // world.cout0("WITH MAP");
  // world.barrier();
  // bbag.map([](std::string s) { return std::make_tuple(s + "!", 5); })
  //     .for_all([&world](std::string s, int i) { world.cout(s, " ", i); });

  //   world.cout0("test flatten");
  // world.barrier();
  // bbag.flatten().for_all([&world](char c) {world.cout(c);});

  // ygm::container::bag<std::string> bbag(
  //     world, {"dog", "cat", "lion", "blue", "snake"});

  // ygm::container::bag<std::string> bag2(world);

  // bbag.collect(bag2);

  // bag2.for_all([&world](std::string s) { world.cout(s); });
  //  bbag.filter([](std::string& s) { s += "!"; return s.size() == 4; })
  //      .for_all([&world](std::string s) { world.cout(s); });

  // const auto& cbag = bbag;
  // cbag.filter([](std::string& s) { s += "!"; return s.size() == 4; })
  //     .for_all([&world](std::string s) { world.cout(s); });

  ygm::container::bag<std::string> bag3(
      world, {"one", "fish", "two", "fish", "red", "fish", "blue", "fish"});

  bag3.for_all([&world](std::string s) { world.cout(s); });

  ygm::container::map<std::string, size_t> word_count(world);

  bag3.filter([](std::string s) { return s.size() == 3; })
      .transform([](std::string s) { return std::make_pair(s, size_t(1)); })
      .reduce_by_key(word_count, std::plus<size_t>());

  word_count.for_all(
      [&world](std::string s, size_t c) { world.cout(s, " ", c); });

  //
  // Compiles but not tested!
  {
    ygm::io::line_parser lp(world, {"dummy"});

    lp.filter([](std::string s) { return s.size() == 3; })
        .transform([](std::string s) { return std::make_pair(s, size_t(1)); })
        .reduce_by_key(word_count, std::plus<size_t>());
  }

  return 0;
}
