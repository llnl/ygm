// Copyright 2019-2024 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#include <unistd.h>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include <arrow/io/file.h>
#include <parquet/stream_writer.h>
#include <boost/json/src.hpp>

#include <ygm/comm.hpp>
#include <ygm/io/csv_parser.hpp>
#include <ygm/io/parquet_parser.hpp>
#include <ygm/utility.hpp>

namespace stdfs = std::filesystem;

struct options_t {
  std::string subcommand;
  std::string input_path;
  bool        read_lines = false;
  std::string output_file_prefix{"output"};
};

static constexpr char const* const ROWCOUNT = "rowcount";
static constexpr char const* const SCHEMA   = "schema";
static constexpr char const* const DUMP     = "dump";
static constexpr char const* const CONVERT  = "convert";

bool parse_arguments(int argc, char** argv, options_t&, bool&);
template <typename os_t>
void show_usage(char** argv, os_t&);
void count_rows(const options_t&, ygm::comm&);
void dump(const options_t&, ygm::comm&);
void convert(const options_t&, ygm::comm&);

int main(int argc, char** argv) {
  ygm::comm world(&argc, &argv);
  {
    options_t opt;
    bool      show_help = false;
    if (!parse_arguments(argc, argv, opt, show_help)) {
      world.cerr0() << "Invalid arguments." << std::endl;
      if (world.rank0()) show_usage(argv, std::cerr);
    }
    if (show_help) {
      if (world.rank0()) show_usage(argv, std::cout);
      return 0;
    }

    if (opt.subcommand == ROWCOUNT) {
      count_rows(opt, world);
    } else if (opt.subcommand == SCHEMA) {
      world.cout0() << "Schema" << std::endl;
      ygm::io::parquet_parser parquetp(world, {opt.input_path.c_str()});
      world.cout0() << parquetp.schema_to_string() << std::endl;
    } else if (opt.subcommand == DUMP) {
      dump(opt, world);
    } else if (opt.subcommand == CONVERT) {
      convert(opt, world);
    } else {
      world.cerr0() << "Unknown subcommand: " << opt.subcommand << std::endl;
    }
  }
  world.barrier();

  return 0;
}

bool parse_arguments(int argc, char** argv, options_t& options,
                     bool& show_help) {
  int opt;
  while ((opt = getopt(argc, argv, "c:i:ro:h")) != -1) {
    switch (opt) {
      case 'c':
        options.subcommand = optarg;
        break;
      case 'i':
        options.input_path = optarg;
        break;
      case 'r':
        options.read_lines = true;
        break;
      case 'o':
        options.output_file_prefix = optarg;
        break;
      case 'h':
        show_help = true;
        break;
      default:
        return false;
    }
  }
  return true;
}

// Only for rank 0
template <typename os_t>
void show_usage(char** argv, os_t& os) {
  os << "[Usage]" << std::endl;
  os << "mpirun -np <#of ranks> ./parquet-tools [options]" << std::endl;
  os << std::endl;

  os << "[Options]" << std::endl;
  os << "  -c <subcommand>" << std::endl;
  os << "    Subcommand name followed by its options." << std::endl;
  os << "  -h Show this help message." << std::endl;
  os << std::endl;

  os << std::endl;
  os << "[Subcommand Options]" << std::endl;

  std::filesystem::path subcommand_file =
      std::filesystem::path(argv[0]).parent_path() /
      "parquet_tools_subcmd.json";
  std::ifstream ifs(subcommand_file);
  std::string   content;
  std::string   line;
  while (getline(ifs, line)) {
    content += line + "\n";
  }
  boost::json::string_view sv(content.c_str());
  boost::json::value       v = boost::json::parse(sv);

  // JSON strings are double-quoted by Boost.JSON.
  // Remove leading and trailing quotes
  auto format = [](boost::json::string bs) {
    std::string s = bs.c_str();
    // Remove leading and trailing whitespace and double quotes
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                                    [](int ch) { return !std::isspace(ch); }));
    return s;
  };

  for (const auto& entry : v.as_array()) {
    const auto& entry_obj = entry.as_object();
    os << format(entry_obj.at("cmd").as_string());
    os << ": " << format(entry_obj.at("desc").as_string()) << std::endl;

    // Required arguments
    if (entry_obj.contains("req")) {
      os << "  Required arguments" << std::endl;
      for (const auto& req : entry_obj.at("req").as_array()) {
        auto& req_obj = req.as_object();
        os << "    -" << format(req_obj.at("key").as_string()) << " ";
        if (req_obj.contains("value")) {
          os << " <" << format(req_obj.at("value").as_string()) << "> ";
        }
        os << format(req_obj.at("desc").as_string()) << std::endl;
      }
    }

    if (entry_obj.contains("opt")) {
      os << "  Optional arguments" << std::endl;
      for (const auto& op : entry_obj.at("opt").as_array()) {
        auto& op_obj = op.as_object();
        assert(op_obj.contains("key"));
        os << "    -" << format(op_obj.at("key").as_string()) << " ";
        if (op_obj.contains("value")) {
          assert(op_obj.contains("value"));
          os << " <" << format(op_obj.at("value").as_string()) << "> ";
        }
        assert(op_obj.contains("desc"));
        os << format(op_obj.at("desc").as_string()) << std::endl;
      }
    }
    os << std::endl;
  }
}

void count_rows(const options_t& opt, ygm::comm& world) {
  world.cout0() << "Count rows, actually reading data." << std::endl;

  ygm::io::parquet_parser parquetp(world, {opt.input_path.c_str()});

  std::size_t num_rows = 0;

  ygm::timer timer{};
  if (opt.read_lines) {
    parquetp.for_all([&num_rows](const auto& row) { ++num_rows; });
    num_rows = world.all_reduce_sum(num_rows);
  } else {
    num_rows = parquetp.num_rows();
  }
  const auto elapsed_time = timer.elapsed();

  world.cout0() << "Elapsed time: " << elapsed_time << " seconds" << std::endl;
  world.cout0() << "#of rows = " << num_rows << std::endl;
}

// Convert a string to CSV format
// Always enclose the string in double quotes
std::string conv2csv(const std::string& str) {
  std::string csv_str;
  csv_str += "\"";
  for (const char& c : str) {
    if (c == '\"') {
      csv_str += "\"";
    }
    csv_str += c;
  }
  csv_str += "\"";
  return csv_str;
}

void dump(const options_t& opt, ygm::comm& world) {
  world.cout0() << "Dump as CSV." << std::endl;

  ygm::io::parquet_parser parquetp(world, {opt.input_path.c_str()});
  const auto&             schema = parquetp.get_schema();

  std::size_t num_rows = 0;

  std::filesystem::path output_path = std::string(opt.output_file_prefix) +
                                      "-" + std::to_string(world.rank()) +
                                      ".csv";
  std::ofstream ofs(output_path);
  if (!ofs) {
    world.cerr0() << "Failed to open the output file: " << output_path
                  << std::endl;
    ::MPI_Abort(world.get_mpi_comm(), EXIT_FAILURE);
  }

  ygm::timer timer{};

  // Write header
  for (int i = 0; i < schema.size(); ++i) {
    ofs << conv2csv(schema[i].name);
    if (i < schema.size() - 1) {
      ofs << ",";
    }
  }
  ofs << "\n";

  // Write data
  parquetp.for_all([&num_rows, &ofs](const auto& row) {
    for (int i = 0; i < row.size(); ++i) {
      std::string str;
      std::visit(
          [&str](auto&& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
              str = "[NA]";
            } else if constexpr (std::is_same_v<T, std::string>) {
              str = value;
            } else {
              str = std::to_string(value);
            }
          },
          row[i]);
      const auto csv_str = conv2csv(str);
      ofs << csv_str;
      if (i < row.size() - 1) {
        ofs << ",";
      }
    }
    ofs << "\n";

    ++num_rows;
  });
  ofs.close();
  if (!ofs) {
    world.cerr0() << "Failed to write the output file: " << output_path
                  << std::endl;
    ::MPI_Abort(world.get_mpi_comm(), EXIT_FAILURE);
  }
  const auto elapsed_time = timer.elapsed();
  num_rows                = world.all_reduce_sum(num_rows);

  world.cout0() << "Elapsed time: " << elapsed_time << " seconds" << std::endl;
  world.cout0() << "#of rows = " << num_rows << std::endl;
}

void convert(const options_t& opt, ygm::comm& world) {
  std::string output_path =
      std::string(opt.output_file_prefix) + "-" + std::to_string(world.rank());
  std::cout << "Output path: " << output_path << std::endl;

  std::filesystem::remove(output_path);
  std::shared_ptr<arrow::io::FileOutputStream> outfile;
  PARQUET_ASSIGN_OR_THROW(outfile,
                          arrow::io::FileOutputStream::Open(output_path));

  parquet::WriterProperties::Builder          builder;
  auto                                        properties = builder.build();
  parquet::schema::NodeVector                 fields;
  std::shared_ptr<parquet::schema::GroupNode> schema = nullptr;

  parquet::StreamWriter parquet_writer;
  ygm::io::csv_parser   csvp(world, std::vector<std::string>{opt.input_path});
  bool                  schema_defined = false;
  csvp.for_all([&parquet_writer, &schema_defined, &fields, &schema, &properties,
                &outfile](const auto& vfields) {
    // Define schema once
    if (!schema_defined) {
      std::size_t col_no = 0;
      for (const auto& f : vfields) {
        std::string                  col_name = "col-" + std::to_string(col_no);
        parquet::Type::type          type(parquet::Type::type::UNDEFINED);
        parquet::ConvertedType::type converted_type(
            parquet::ConvertedType::NONE);
        if (f.is_integer()) {
          type           = parquet::Type::type::INT64;
          converted_type = parquet::ConvertedType::INT_64;
        } else if (f.is_unsigned_integer()) {
          const uint64_t v = f.as_unsigned_integer();
          type             = parquet::Type::type::INT64;
          converted_type   = parquet::ConvertedType::UINT_64;
        } else if (f.is_double()) {
          type           = parquet::Type::type::DOUBLE;
          converted_type = parquet::ConvertedType::NONE;
        } else {
          type           = parquet::Type::type::BYTE_ARRAY;
          converted_type = parquet::ConvertedType::UTF8;
        }
        fields.push_back(parquet::schema::PrimitiveNode::Make(
            col_name, parquet::Repetition::REQUIRED, type, converted_type));
        ++col_no;
      }
      schema = std::static_pointer_cast<parquet::schema::GroupNode>(
          parquet::schema::GroupNode::Make(
              "schema", parquet::Repetition::REQUIRED, fields));
      parquet_writer = parquet::StreamWriter{
          parquet::ParquetFileWriter::Open(outfile, schema, properties)};
      schema_defined = true;
    }

    for (auto f : vfields) {
      if (f.is_integer()) {
        const int64_t v = f.as_integer();
        parquet_writer << v;
      } else if (f.is_unsigned_integer()) {
        const int64_t v = f.as_unsigned_integer();
        parquet_writer << v;
      } else if (f.is_double()) {
        parquet_writer << f.as_double();
      } else {
        parquet_writer << f.as_string();
      }
    }
    parquet_writer << parquet::EndRow;
  });
  if (schema_defined) {
    parquet_writer << parquet::EndRowGroup;
  }
}