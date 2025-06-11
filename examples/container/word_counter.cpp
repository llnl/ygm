// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#include <map>
#include <sstream>
#include <ygm/comm.hpp>
#include <ygm/container/counting_set.hpp>

// President Abraham Lincoln's Gettysburg Address,
// with punctuation and capitalization removed.
// Ref:  https://en.wikipedia.org/wiki/Gettysburg_Address
const char *gettysburg =
    "four score and seven years ago our fathers brought forth on this "
    "continent a new nation conceived in liberty and dedicated to the "
    "proposition that all men are created equal now we are engaged in a great "
    "civil war testing whether that nation or any nation so conceived and so "
    "dedicated can long endure we are met on a great battle field of that war "
    "we have come to dedicate a portion of that field as a final resting place "
    "for those who here gave their lives that that nation might live it is "
    "altogether fitting and proper that we should do this but in a larger "
    "sense we can not dedicate we can not consecrate we can not hallow this "
    "ground the brave men living and dead who struggled here have consecrated "
    "it far above our poor power to add or detract the world will little note "
    "nor long remember what we say here but it can never forget what they did "
    "here it is for us the living rather to be dedicated here to the "
    "unfinished work which they who fought here have thus far so nobly "
    "advanced it is rather for us to be here dedicated to the great task "
    "remaining before us that from these honored dead we take increased "
    "devotion to that cause for which they gave the last full measure of "
    "devotion that we here highly resolve that these dead shall not have died "
    "in vain that this nation under god shall have a new birth of freedom and "
    "that government of the people by the people for the people shall not "
    "perish from the earth";

int main(int argc, char **argv) {
  ygm::comm world(&argc, &argv);

  auto        iss = std::istringstream{gettysburg};
  std::string word;

  ygm::container::counting_set<std::string> word_counter(world);

  // pragma parallel
  while (iss >> word) {
    word_counter.async_insert(word);
  }

  //
  // Setup gather lists, each rank can gather independently
  std::vector<std::string> to_gather;
  if (world.rank() == 0) {
    to_gather = {"government"};
  }
  if (world.rank() == 1) {
    to_gather = {"people"};
  }
  if (world.rank() == 2) {
    to_gather = {"freedom"};
  }

  auto counts = word_counter.gather_keys(to_gather);

  for (auto &word_count : counts) {
    std::cout << word_count.first << " -> " << word_count.second << std::endl;
  }

  return 0;
}
