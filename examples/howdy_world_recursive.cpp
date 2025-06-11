// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#include <ygm/comm.hpp>

// Define the function to execute recursively
struct howdy {
  template <typename Comm>
  void operator()(Comm* pcomm, int from, const std::string& str) {
    std::cout << "Howdy, I'm rank " << pcomm->rank()
              << ", and I received a message from rank " << from
              << " that read: \"" << str << "\"" << std::endl;
    if (pcomm->rank() + 1 < pcomm->size()) {
      pcomm->async(pcomm->rank() + 1, howdy(), pcomm->rank(),
                   std::string("This was sent recursively"));
    }
  }
};

int main(int argc, char** argv) {
  ygm::comm world(&argc, &argv);

  if (world.rank() == 0 && world.size() > 1) {
    world.async(1, howdy(), world.rank(), std::string("Can you hear me now?"));
  }

  return 0;
}
