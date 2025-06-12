// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG

#include <ygm/comm.hpp>

int main(int argc, char** argv) {
  ygm::comm world(&argc, &argv);

  {  // testing comm's ostream helpers
    world.cout() << "cout all ranks." << std::endl;
    world.cerr() << "cerr all ranks." << std::endl;
    world.cout0() << "cout only rank 0." << std::endl;
    world.cerr0() << "cerr only rank 0." << std::endl;

    world.cout("variadic cout all ranks.");
    world.cerr("variadic cerr all ranks.");
    world.cout0("variadic cout only rank 0.");
    world.cerr0("variadic cerr only rank 0.");
  }

  {  // testing ygm::'s ostream helpers
    ygm::wcout() << "cout all ranks." << std::endl;
    ygm::wcerr() << "cerr all ranks." << std::endl;
    ygm::wcout0() << "cout only rank 0." << std::endl;
    ygm::wcerr0() << "cerr only rank 0." << std::endl;

    ygm::wcout("variadic cout all ranks.");
    ygm::wcerr("variadic cerr all ranks.");
    ygm::wcout0("variadic cout only rank 0.");
    ygm::wcerr0("variadic cerr only rank 0.");
  }
}