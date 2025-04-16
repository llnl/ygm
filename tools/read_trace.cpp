#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/variant.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <variant>
#include <ygm/detail/tracer.hpp>

using namespace ygm::detail;
namespace fs = std::filesystem;

void deserializeFromFile(const std::string& filename) {
  std::ifstream is(filename, std::ios::binary);
  if (!is) {
    std::cerr << "Failed to open file for reading: " << filename << std::endl;
    return;
  }

  cereal::BinaryInputArchive iarchive(is);
  variant_event              variant_event{};

  bool first_entry = true;

  // Loop to deserialize and print events until the end of the file
  while (is.peek() != EOF) {
    iarchive(variant_event);

    // Add comma between JSON objects, but not before the first one
    if (!first_entry) {
      std::cout << ",\n";
    } else {
      first_entry = false;
    }

    std::visit(
        [](auto&& arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, ygm_async_event>) {
            std::cout << "{"
                      << "\"type\":\"YGM_ASYNC\","
                      << "\"event_id\":" << arg.event_id << ","
                      << "\"to\":" << arg.to << ","
                      << "\"message_size\":" << arg.message_size << "}";
          } else if constexpr (std::is_same_v<T, mpi_send_event>) {
            std::cout << "{"
                      << "\"type\":\"MPI_SEND\","
                      << "\"event_id\":" << arg.event_id << ","
                      << "\"to\":" << arg.to << ","
                      << "\"buffer_size\":" << arg.buffer_size << "}";
          } else if constexpr (std::is_same_v<T, mpi_send_complete_event>) {
            std::cout << "{"
                      << "\"type\":\"MPI_SEND_COMPLETE\","
                      << "\"event_id\":" << arg.event_id << ","
                      << "\"start_id\":" << arg.start_id << ","
                      << "\"buffer_size\":" << arg.buffer_size << "}";
          } else if constexpr (std::is_same_v<T, mpi_recv_event>) {
            std::cout << "{"
                      << "\"type\":\"MPI_RECV\","
                      << "\"event_id\":" << arg.event_id << ","
                      << "\"from\":" << arg.from << ","
                      << "\"buffer_size\":" << arg.buffer_size << "}";
          } else if constexpr (std::is_same_v<T, barrier_begin_event>) {
            std::cout << "{"
                      << "\"type\":\"BARRIER_BEGIN\","
                      << "\"event_id\":" << arg.event_id << ","
                      << "\"send_count\":" << arg.send_count << ","
                      << "\"recv_count\":" << arg.recv_count << ","
                      << "\"pending_isend_bytes\":" << arg.pending_isend_bytes
                      << ","
                      << "\"send_local_buffer_bytes\":"
                      << arg.send_local_buffer_bytes << ","
                      << "\"send_remote_buffer_bytes\":"
                      << arg.send_remote_buffer_bytes << "}";
          } else if constexpr (std::is_same_v<T, barrier_end_event>) {
            std::cout << "{"
                      << "\"type\":\"BARRIER_END\","
                      << "\"event_id\":" << arg.event_id << ","
                      << "\"send_count\":" << arg.send_count << ","
                      << "\"recv_count\":" << arg.recv_count << ","
                      << "\"pending_isend_bytes\":" << arg.pending_isend_bytes
                      << ","
                      << "\"send_local_buffer_bytes\":"
                      << arg.send_local_buffer_bytes << ","
                      << "\"send_remote_buffer_bytes\":"
                      << arg.send_remote_buffer_bytes << "}";
          }
        },
        variant_event.data);
  }
}

void printUsage(const char* progName) {
  std::cerr << "Usage:\n\n"
            << "  " << progName << " --file <filename>\n"
            << "  or\n"
            << "  " << progName << " --directory <directory_name>\n"
            << "\nExactly one of --file/-f or --directory/-d must be used.\n";
}

int main(int argc, char* argv[]) {
  bool        file_provided      = false;
  bool        directory_provided = false;
  std::string filename;
  std::string directory;

  // Parse command-line arguments:
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if ((arg == "--file" || arg == "-f") && (i + 1 < argc)) {
      filename      = argv[++i];
      file_provided = true;
    } else if ((arg == "--directory" || arg == "-d") && (i + 1 < argc)) {
      directory          = argv[++i];
      directory_provided = true;
    } else {
      printUsage(argv[0]);
      return 1;
    }
  }

  if ((file_provided && directory_provided) ||
      (!file_provided && !directory_provided)) {
    printUsage(argv[0]);
    return 1;
  }

  if (file_provided) {
    deserializeFromFile(filename);
  } else if (directory_provided) {
    if (!fs::is_directory(directory)) {
      std::cerr << "Not a valid directory: " << directory << std::endl;
      return 1;
    }

    bool first_file = true;
    for (auto& entry : fs::directory_iterator(directory)) {
      if (entry.is_regular_file() && entry.path().extension() == ".bin") {
        if (!first_file) {
          // Add separator between files if needed
          std::cout << "\n";
        } else {
          first_file = false;
        }
        deserializeFromFile(entry.path().string());
      }
    }
  }

  return 0;
}