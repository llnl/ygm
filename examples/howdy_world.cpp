// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#include <ygm/comm.hpp>

int main(int argc, char** argv) {
  ygm::comm world(&argc, &argv);

  // Define the function to execute in async
  auto howdy = [](auto pcomm, int from, const std::string& str) {
    std::cout << "Howdy, I'm rank " << pcomm->rank()
              << ", and I received a message from rank " << from
              << " that read: \"" << str << "\"" << std::endl;
  };

  if (world.rank() == 0) {
    for (int dest = 0; dest < world.size(); ++dest) {
      world.async(dest, howdy, world.rank(),
                  std::string("Can you hear me now?"));
    }
  }
  return 0;
}
