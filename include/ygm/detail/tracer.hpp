// Copyright 2019-2021 Lawrence Livermore National Security, LLC and other YGM
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
  std::variant<ygm_async_event, mpi_send_event, mpi_recv_event,
               barrier_begin_event, barrier_end_event>
      data{};
  template <class Archive>
  void serialize(Archive& archive) {
    archive(data);
  }
};

class tracer {
 public:
  tracer() = default;

  ~tracer() {
    if (output_file.is_open()) {
      output_file.close();
      if (output_file.fail()) {
        std::cerr << "Error closing trace file!" << std::endl;
      }
    }
  }

  void create_directory(const std::string& trace_path) {
    if (!std::filesystem::is_directory(trace_path)) {
      if (!std::filesystem::create_directories(trace_path)) {
        std::cerr << "Error creating directory!" << std::endl;
      }
    }
  }

  void open_file(const std::string& trace_path, int comm_rank, int comm_size) {
    m_comm_size = comm_size;
    m_rank      = comm_rank;
    std::string file_path =
        trace_path + "/trace_" + std::to_string(comm_rank) + ".bin";
    output_file.open(file_path, std::ios::binary);

    if (!output_file.is_open()) {
      std::cerr << "Error opening " << file_path << " for writing!"
                << std::endl;
    }
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
};

}  // namespace ygm::detail
