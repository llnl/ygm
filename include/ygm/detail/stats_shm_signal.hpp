// Copyright 2019-2026 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

/**
 * @file stats_shm_signal.hpp
 * @brief Process-wide signal handling for shm stats cleanup on abnormal exit.
 *
 * Purpose
 *   YGM's comm_stats can back its counters with a POSIX shared memory
 *   segment (see comm_stats::open_shm). Those segments live in /dev/shm
 *   and are normally unlinked when the owning comm is destroyed, but a
 *   signal-terminated process would otherwise leak them. This module
 *   stores the tracked signals and installs a chained handler that
 *   calls shm_unlink on each segment before forwarding to the previously
 *   installed handler and re-raising with the default disposition.
 */

#pragma once

#include <ygm/detail/ygm_uuids.hpp>

#include <csignal>
#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>

namespace ygm::detail::shm {

/* STORAGE AND CONSTANTS FOR SIG HANDLING */

constexpr char shm_prefix[] = "/ygm_";

// One-shot guard on handler installation.
inline bool process_signal_handlers_registered = false;

// Signals intercepted for shm cleanup on abnormal exit. SIGKILL and
// SIGSTOP are not present by design, they cannot be caught. SIGPIPE
// is occasionally used by MPI on TCP transports; remove it here if that
// causes double-handling issues with MPI implementation. (Early abort)
constexpr int tracked_signals[] = {
    SIGHUP,  SIGINT,   SIGQUIT, SIGILL,   SIGTRAP,
    SIGABRT, SIGBUS,   SIGFPE,  SIGSEGV,  SIGPIPE,
    SIGTERM, SIGSYS, SIGXCPU, SIGXFSZ, SIGVTALRM,
    #ifdef __linux__ 
    SIGPWR,  SIGSTKFLT 
    #endif
    #ifdef __APPLE__ 
    SIGEMT
    #endif
};

constexpr size_t num_tracked_signals =
    sizeof(tracked_signals) / sizeof(tracked_signals[0]);

// Previous handlers, parallel to tracked_signals[]. Populated by
// ensure_handlers_registered().
inline struct sigaction old_actions[num_tracked_signals];

/* HANDLING AND SUPPORT FUNCTIONS */

inline void chained_unlink_handler(int sig, siginfo_t* info,
                                   void* ucontext) {
  constexpr char prefix_msg[] = "Caught signal ";
  constexpr char suffix_msg[] =
      " in chained handler. Initiating unlink for ygm shm segments.\n";
  char signum[2] = {static_cast<char>(sig / 10 % 10 + '0'),
                    static_cast<char>(sig % 10 + '0')};

  // sizeof(msg)-1 for null term strings. Keeps byte count synced with msg length
  // (void)! cast is warning supression; return not material if already in failure mode
  (void)!write(STDOUT_FILENO, prefix_msg, sizeof(prefix_msg) - 1);
  (void)!write(STDOUT_FILENO, signum, 2);
  (void)!write(STDOUT_FILENO, suffix_msg, sizeof(suffix_msg) - 1);

  // Create substrate to copy path_ids onto
  char shm_path[64];

  for (const std::string& path_id : ygm::detail::live_comm_uuids) {
    // Construct Desired String Format: <shm_prefix><path_id+"\0"><remaining_endspace>
    memset(shm_path, 0, sizeof(shm_path));
    memcpy(shm_path, shm_prefix, strlen(shm_prefix));
    strncpy(shm_path + (sizeof(shm_prefix) - 1), path_id.c_str(), path_id.size() + 1);

    shm_unlink(shm_path);
  }

  // Forward to the previously installed handler.
  for (size_t i = 0; i < num_tracked_signals; ++i) {
    if (tracked_signals[i] == sig) {
      // MPI implementations commonly install SA_SIGINFO handlers on
      // SIGSEGV/SIGBUS/SIGFPE for backtrace machinery.
      // Use sa_sigaction instead of sa_handler.
      if (old_actions[i].sa_flags & SA_SIGINFO) {
        if (old_actions[i].sa_sigaction != nullptr) {
          old_actions[i].sa_sigaction(sig, info, ucontext);
        }
      } 
      else if (old_actions[i].sa_handler != SIG_DFL &&
                 old_actions[i].sa_handler != SIG_IGN) {
        old_actions[i].sa_handler(sig);
      }
      break;
    }
  }

  // Restore default disposition and re-raise so the process terminates
  // with the correct signal / exit status.
  signal(sig, SIG_DFL);
  raise(sig);
}

inline void ensure_handlers_registered() {
  // Lazy install on first use.
  if (process_signal_handlers_registered) return;
  process_signal_handlers_registered = true;

  // Default (no SA_NODEFER): POSIX blocks the same signal during its own
  // handler, so the handler does not re-enter itself on the active rank.
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_flags     = SA_SIGINFO;
  sa.sa_sigaction = chained_unlink_handler;
  sigemptyset(&sa.sa_mask);

  for (size_t i = 0; i < num_tracked_signals; ++i) {
    if (sigaction(tracked_signals[i], &sa, &old_actions[i]) < 0) {
      perror("ygm: sigaction registration failure.\n");
    }
  }
}

}  // namespace ygm::detail::shm
