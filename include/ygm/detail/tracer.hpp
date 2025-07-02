// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/variant.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <variant>

namespace ygm::detail {

// YGM Async
struct ygm_async_event {
  uint64_t event_id;
  int      to;
  uint32_t message_size;

  template <class Archive>
  void serialize(Archive& ar) {
    ar(event_id, to, message_size);
  }
};

// MPI Send
struct mpi_send_event {
  uint64_t event_id;
  int      to;
  uint32_t buffer_size;

  template <class Archive>
  void serialize(Archive& ar) {
    ar(event_id, to, buffer_size);
  }
};

// MPI SEND COMPLETE
struct mpi_send_complete_event {
  uint64_t event_id;
  uint64_t start_id;
  uint32_t buffer_size;

  template <class Archive>
  void serialize(Archive& ar) {
    ar(event_id, start_id, buffer_size);
  }
};

// MPI Receive
struct mpi_recv_event {
  uint64_t event_id;
  int      from;
  uint32_t buffer_size;

  template <class Archive>
  void serialize(Archive& ar) {
    ar(event_id, from, buffer_size);
  }
};

// Barrier Begin
struct barrier_begin_event {
  uint64_t event_id;
  uint64_t send_count;
  uint64_t recv_count;
  size_t   pending_isend_bytes;
  size_t   send_local_buffer_bytes;
  size_t   send_remote_buffer_bytes;

  template <class Archive>
  void serialize(Archive& ar) {
    ar(event_id, send_count, recv_count, pending_isend_bytes,
       send_local_buffer_bytes, send_remote_buffer_bytes);
  }
};

// Barrier End
struct barrier_end_event {
  uint64_t event_id;
  uint64_t send_count;
  uint64_t recv_count;
  size_t   pending_isend_bytes;
  size_t   send_local_buffer_bytes;
  size_t   send_remote_buffer_bytes;

  template <class Archive>
  void serialize(Archive& ar) {
    ar(event_id, send_count, recv_count, pending_isend_bytes,
       send_local_buffer_bytes, send_remote_buffer_bytes);
  }
};

struct variant_event {
  std::variant<ygm_async_event, mpi_send_event, mpi_send_complete_event,
               mpi_recv_event, barrier_begin_event, barrier_end_event>
      data{};
  template <class Archive>
  void serialize(Archive& archive) {
    archive(data);
  }
};

class tracer {
 public:
  tracer(int comm_size, int rank, const std::string& trace_path) {
    m_comm_size       = comm_size;
    m_rank            = rank;
    m_next_message_id = m_rank - m_comm_size;
    m_trace_path      = trace_path;
  }

  ~tracer() { close_file(); }

  // Check if file is already open
  bool is_file_open() const { return output_file.is_open(); }

  // Create directory if it doesn't exist
  bool create_directory() {
    if (m_rank == 0) {
      if (!std::filesystem::is_directory(m_trace_path)) {
        try {
          if (!std::filesystem::create_directories(m_trace_path)) {
            std::cerr << "Error creating directory: " << m_trace_path
                      << std::endl;
            return false;
          }
        } catch (const std::filesystem::filesystem_error& e) {
          std::cerr << "Filesystem error creating directory: " << e.what()
                    << std::endl;
          return false;
        }
      }
    }
    return true;
  }

  // Open trace file if not already open
  bool open_file() {
    if (output_file.is_open()) {
      return true;  // Already open
    }

    std::string file_path =
        m_trace_path + "/trace_" + std::to_string(m_rank) + ".bin";
    output_file.open(file_path, std::ios::binary);

    if (!output_file.is_open()) {
      std::cerr << "Error opening " << file_path << " for writing!"
                << std::endl;
      return false;
    }
    return true;
  }

  // Close trace file if open
  bool close_file() {
    if (output_file.is_open()) {
      output_file.close();
      if (output_file.fail()) {
        std::cerr << "Error closing trace file!" << std::endl;
        return false;
      }
    }
    return true;
  }

  // Function to generate the next unique message id
  int get_next_message_id() { return m_next_message_id += m_comm_size; }

  // Loging an event
  template <typename EventType>
  void log_event(const EventType& event) {
    cereal::BinaryOutputArchive oarchive(output_file);
    variant_event               variant_event{event};
    oarchive(variant_event);
  }

  void trace_ygm_async(uint64_t id, int dest, uint32_t bytes) {
    ygm_async_event event;
    event.event_id     = id;
    event.to           = dest;
    event.message_size = bytes;

    log_event(event);
  }

  void trace_mpi_send_complete(uint64_t id, uint64_t start_id, uint32_t bytes) {
    mpi_send_complete_event event;
    event.event_id    = id;
    event.start_id    = start_id;
    event.buffer_size = bytes;

    log_event(event);
  }

  void trace_mpi_send(uint64_t id, int dest, uint32_t bytes) {
    mpi_send_event event;
    event.event_id    = id;
    event.to          = dest;
    event.buffer_size = bytes;

    log_event(event);
  }

  void trace_mpi_recv(uint64_t id, int from, uint32_t bytes) {
    mpi_recv_event event;
    event.event_id    = id;
    event.from        = from;
    event.buffer_size = bytes;

    log_event(event);
  }

  void trace_barrier_begin(uint64_t id, uint64_t send_count,
                           uint64_t recv_count, size_t pending_isend_bytes,
                           size_t send_local_buffer_bytes,
                           size_t send_remote_buffer_bytes) {
    barrier_begin_event event;
    event.event_id                 = id;
    event.send_count               = send_count;
    event.recv_count               = recv_count;
    event.pending_isend_bytes      = pending_isend_bytes;
    event.send_local_buffer_bytes  = send_local_buffer_bytes;
    event.send_remote_buffer_bytes = send_remote_buffer_bytes;

    log_event(event);
  }

  void trace_barrier_end(uint64_t id, uint64_t send_count, uint64_t recv_count,
                         size_t pending_isend_bytes,
                         size_t send_local_buffer_bytes,
                         size_t send_remote_buffer_bytes) {
    barrier_end_event event;
    event.event_id                 = id;
    event.send_count               = send_count;
    event.recv_count               = recv_count;
    event.pending_isend_bytes      = pending_isend_bytes;
    event.send_local_buffer_bytes  = send_local_buffer_bytes;
    event.send_remote_buffer_bytes = send_remote_buffer_bytes;

    log_event(event);
  }

 private:
  std::ofstream output_file;
  int           m_comm_size       = 0;
  int           m_rank            = -1;
  int           m_next_message_id = 0;
  std::string   m_trace_path;
};

}  // namespace ygm::detail
