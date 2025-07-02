// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <ygm/comm.hpp>

namespace ygm {

namespace detail {

class interrupt_mask {
 public:
  interrupt_mask(ygm::comm &c) : m_comm(c) {
    m_comm.m_enable_interrupts = false;
  }

  ~interrupt_mask() {
    m_comm.m_enable_interrupts = true;
    m_comm.check_if_production_halt_required();
    // m_comm.process_receive_queue();  //causes recursion into
    // process_receive_queue
  }

 private:
  ygm::comm &m_comm;
};

}  // namespace detail
}  // namespace ygm
