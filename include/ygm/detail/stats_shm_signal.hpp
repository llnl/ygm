// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

/**
 * TODOs: 
 *      * Review Lines 125 - 160 (end of handler and all ensure_ensure registered)
 *      * Rewrite blurb at top of file
 */

/**
 * @file stats_shm_signal.hpp
 * @brief Process-wide signal handling for shm stats cleanup on abnormal exit.
 *
 * Purpose
 *   YGM's comm_stats can back its counters with a POSIX shared memory
 *   segment (see comm_stats::open_shm). Those segments live in /dev/shm
 *   and are normally unlinked when the owning comm is destroyed, but a
 *   signal-terminated process would otherwise leak them. This module
 *   installs a chained signal handler that walks a table of staged shm
 *   paths and calls shm_unlink on each before forwarding to the
 *   previously installed handler and re-raising with the default
 *   disposition.
 *
 * Scope and linkage
 *   State here is process-global (one signal disposition per process),
 *   not per-comm_stats, so the tracking array, old_actions table, and
 *   handler live at namespace scope rather than as class members. All
 *   mutable storage is `inline` so every TU that includes this header
 *   shares a single definition.
 *
 * Lifecycle
 *   - comm_stats::open_shm calls register_path(path); the first call in
 *     a process lazily installs handlers for every tracked signal. The
 *     path is stored verbatim, so the handler only performs an
 *     async-signal-safe shm_unlink per live entry.
 *   - comm_stats::~comm_stats calls unregister_path(path) after unlinking
 *     the segment, so a signal firing mid-teardown sees either an
 *     already-unlinked path (ENOENT) or the live tail of the array.
 *   - Signal handlers are never uninstalled; once present they stay for
 *     the life of the process.
 *
 * Async-signal-safety
 *   The handler uses only async-signal-safe syscalls (write, shm_unlink,
 *   signal, raise). No std::string, no heap, no stdio -- the path is
 *   supplied by the caller and stored up front, so the handler only
 *   dereferences a fixed-size buffer. The handler registers with
 *   SA_SIGINFO so it can forward (siginfo_t*, ucontext*) to previously
 *   installed handlers that used the three-argument form -- common with
 *   MPI backtrace machinery on SIGSEGV/SIGBUS/SIGFPE.
 */

#pragma once

#ifdef __linux__

#include <csignal>
#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>

namespace ygm::detail::shm {

/* STORAGE AND CONSTANTS FOR SIG HANDLING */

// Max # of concurrent shm paths that can be tracked for signal cleanup.
// Slots released at comm_stats destruction
constexpr int MAX_SHM_PATHS = 8;

// Max shm path: "/ygm_" (5) + uuid (36) + "_rank" (5) + int32 digits (11 w/ sign) + NUL.
constexpr size_t kShmPathMaxLen = 64;

// Tracking array. Mutated by register/unregister_path in main thread;
// read by signal handler to unlink in case of abnormal behavior.
inline int  num_active_paths                                = 0;
inline char active_shm_paths[MAX_SHM_PATHS][kShmPathMaxLen] = {0};

// One-shot guard on handler installation.
inline bool process_signal_handlers_registered = false;

// Signals intercepted for shm cleanup on abnormal exit. SIGKILL and
// SIGSTOP are not present by design, they cannot be caught. SIGPIPE
// is occasionally used by MPI on TCP transports; remove it here if that
// causes double-handling issues with MPI implementation. (Early abort)
constexpr int tracked_signals[] = {
    SIGHUP,  SIGINT,   SIGQUIT, SIGILL,   SIGTRAP,
    SIGABRT, SIGBUS,   SIGFPE,  SIGSEGV,  SIGPIPE,
    SIGTERM, SIGSTKFLT, SIGXCPU, SIGXFSZ, SIGVTALRM,
    SIGPWR,  SIGSYS
};

constexpr size_t num_tracked_signals =
    sizeof(tracked_signals) / sizeof(tracked_signals[0]);

// Previous handlers, parallel to tracked_signals[]. Populated by
// ensure_handlers_registered().
inline struct sigaction old_actions[num_tracked_signals];


/* HANDLING AND SUPPORT FUNCTIONS BELOW */

inline void chained_unlink_handler(int sig, siginfo_t* info,
                                   void* ucontext) {
  // Diagnostic via async-signal-safe write().
  constexpr char prefix_msg[] = "Caught signal ";
  constexpr char suffix_msg[] =
      " in chained handler. Initiating unlink for ygm shm segments.\n";
  char signum[2] = {static_cast<char>(sig / 10 % 10 + '0'),
                    static_cast<char>(sig % 10 + '0')};

  // sizeof(msg)-1 for null term strings. Keeps byte count synced with msg length
  // (void)! cast is kind of hacky warning supression, but if fails all is lost
  (void)!write(STDOUT_FILENO, prefix_msg, sizeof(prefix_msg) - 1);
  (void)!write(STDOUT_FILENO, signum, 2);
  (void)!write(STDOUT_FILENO, suffix_msg, sizeof(suffix_msg) - 1);

  // Snapshot count so a concurrent unregister swap+decrement doesn't
  // create a skipped path
  const int n = num_active_paths;
  for (int i = 0; i < n; ++i) {
    shm_unlink(active_shm_paths[i]);
  }

  // Forward to the previously installed handler.
  for (size_t i = 0; i < num_tracked_signals; ++i) {
    if (tracked_signals[i] == sig) {
      // MPI implementations commonly install SA_SIGINFO handlers on
      // SIGSEGV/SIGBUS/SIGFPE for backtrace machinery. Calling
      // sa_handler would invoke the wrong union member.
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

// Stage a shm path for signal-handler cleanup. Called before a
// segment is created so a live segment always has a tracking entry. 
inline bool register_path(const char* path) {
  ensure_handlers_registered();
  if (num_active_paths == MAX_SHM_PATHS) {
    return false;
  }
  const size_t len = strlen(path);
  memcpy(active_shm_paths[num_active_paths], path, len + 1);
  num_active_paths++;
  return true;
}

// Remove a path from the signal-tracking array via swap-and-decrement.
// Caller MUST have already unlinked the segment prior to unregistering.
// The handler snapshots num_active_paths once, so a signal firing during
// this function sees at worst a brief duplicate (slot i and the tail
// slot holding the same path) which reduces to an extra shm_unlink that
// returns ENOENT -- harmless. The caller must have already unlinked the
// backing segment; otherwise a signal firing between the swap and the
// decrement could leave a live segment at the vacated slot.
inline void unregister_path(const char* path) {
  for (int i = 0; i < num_active_paths; ++i) {
    if (strcmp(active_shm_paths[i], path) == 0) {
      const int last = num_active_paths - 1;
      if (i != last) {
        memcpy(active_shm_paths[i], active_shm_paths[last], kShmPathMaxLen);
      }
      num_active_paths--;
      return;
    }
  }
}

}  // namespace ygm::detail::shm

#endif  // __linux__
