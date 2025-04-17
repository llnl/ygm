// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

#include <ygm/comm.hpp>
#include <ygm/detail/ygm_ptr.hpp>

#include <arrow/api.h>
#include <arrow/io/file.h>
#include <arrow/util/logging.h>

#include <parquet/api/reader.h>
#include <parquet/exception.h>
#include <parquet/metadata.h>
#include <parquet/stream_reader.h>
#include <parquet/types.h>

namespace stdfs = std::filesystem;

namespace ygm::io {
namespace detail {

// List of supported Parquet::Type types
// parquet::Type::FIXED_LEN_BYTE_ARRAY is not supported
constexpr std::array<parquet::Type::type, 6> supported_parquet_types = {
    parquet::Type::BOOLEAN, parquet::Type::INT32,  parquet::Type::INT64,
    parquet::Type::FLOAT,   parquet::Type::DOUBLE, parquet::Type::BYTE_ARRAY,
};

/// Check if the data type is supported by this parser.
inline constexpr bool is_supported_parquet_type(
    const parquet::Type::type type) {
  for (const auto &supported_type : supported_parquet_types) {
    if (type == supported_type) {
      return true;
    }
  }
  return false;
}

/// Holds parquet data type information.
/// Just a utility wrapper around parquet::Type::type.
struct parquet_data_type {
  parquet::Type::type type{parquet::Type::type::UNDEFINED};

  bool equal(const parquet::Type::type other_type) const {
    return other_type == type;
  }

  /// Check if the data type is supported by this parser.
  bool supported() const { return is_supported_parquet_type(type); }

  friend std::ostream &operator<<(std::ostream &, const parquet_data_type &);
};

std::ostream &operator<<(std::ostream &os, const parquet_data_type &t) {
  os << parquet::TypeToString(t.type);
  if (!t.supported()) {
    os << " (unsupported)";
  }
  return os;
}
}  // namespace detail

// Parquet file parser
// Only supports the plain encoding.
// Do not support nested or hierarchical columns.
class parquet_parser {
 private:
  using self_type = parquet_parser;

 public:
  // List of C++ types supported by this parser
  // Note: there is no uint types in Parquet
  using parquet_type_variant =
      std::variant<std::monostate, bool, int32_t, int64_t, float, double,
                   std::string>;

  struct column_schema_type {
    detail::parquet_data_type type;
    std::string               name;
    // If true, this parser can not handle this column data.
    // This can happen due to unsupported data type or non-flat column structure.
    bool unsupported{false};
  };

  parquet_parser(ygm::comm &_comm, const std::vector<std::string> &stringpaths,
                 const bool recursive = false)
      : m_comm(_comm), pthis(this) {
    pthis.check(m_comm);
    init(stringpaths, recursive);
  }

  ~parquet_parser() { m_comm.barrier(); }

  // Returns a list of column schema (simpler version).
  // The order of the schema is the same as the order of Parquet column
  // indices (ascending order).
  // This function assumes that all files have the same schema.
  const std::vector<column_schema_type> &get_schema() { return m_col_schema; }

  // Return full Parquet file schema directly returned by the Parqet reader as a
  // string. This function assumes that all files have the same schema.
  const std::string &schema_to_string() { return m_schema_string; }

  /// Read all rows and call the function for each row.
  /// fn is called with a std::vector<parquet_type_variant>.
  /// The value of an unsupported column is set to std::monostate.
  /// num_rows: Max number of rows the rank to read.
  template <typename Function>
    requires std::invocable<Function, const std::vector<parquet_type_variant> &>
  void for_all(Function     fn,
               const size_t num_rows = std::numeric_limits<size_t>::max()) {
    read_parquet_files(fn, num_rows);
  }

  /// for_all(), read only the specified columns.
  template <typename Function>
    requires std::invocable<Function, const std::vector<parquet_type_variant> &>
  void for_all(const std::vector<std::string> &columns, Function fn,
               const size_t num_rows = std::numeric_limits<size_t>::max()) {
    read_parquet_files(fn, num_rows, columns);
  }

  // Return the number of total files
  size_t num_files() { return m_paths.size(); }

  // Return the number of rows in all files
  size_t num_rows() { return m_num_total_rows; }

 private:
  // Open Parquet files and read schema.
  void init(const std::vector<std::string> &stringpaths,
            const bool                      recursive = false) {
    clear();
    check_paths(stringpaths, recursive);
    read_file_schema();
    count_all_rows();
    m_comm.barrier();
  }

  // Clean up the internal state.
  void clear() {
    m_paths.clear();
    m_col_schema.clear();
    m_num_total_rows = 0;
  }

  /// Count the number of lines in a file.
  static size_t get_num_rows(const stdfs::path &input_filename) {
    std::shared_ptr<arrow::io::ReadableFile> input_file;
    PARQUET_ASSIGN_OR_THROW(input_file,
                            arrow::io::ReadableFile::Open(input_filename));
    std::unique_ptr<parquet::ParquetFileReader> parquet_file_reader =
        parquet::ParquetFileReader::Open(input_file);

    std::shared_ptr<parquet::FileMetaData> file_metadata =
        parquet_file_reader->metadata();
    const size_t num_rows = file_metadata->num_rows();

    return num_rows;
  }

  void count_all_rows() {
    size_t total_rows = 0;
    for (size_t i = 0; i < m_paths.size(); ++i) {
      if (is_owner(i)) {
        total_rows += get_num_rows(m_paths[i]);
      }
    }
    m_num_total_rows = m_comm.all_reduce_sum(total_rows);
  }

  /// Open a Parquet file and return a ParquetFileReader object.
  static std::unique_ptr<parquet::ParquetFileReader> open_file(
      const stdfs::path &input_filename) {
    // Open the Parquet file
    std::shared_ptr<arrow::io::ReadableFile> input_file;
    try {
      PARQUET_ASSIGN_OR_THROW(input_file,
                              arrow::io::ReadableFile::Open(input_filename));
    } catch (...) {
      return nullptr;
    }
    // Create a ParquetFileReader object
    return parquet::ParquetFileReader::Open(input_file);
  }

  /**
   * @brief Check readability of paths and iterates through directories
   *
   * @param stringpaths
   * @param recursive
   */
  void check_paths(const std::vector<std::string> &stringpaths,
                   bool                            recursive) {
    if (m_comm.rank0()) {
      std::vector<std::string> good_stringpaths;

      for (const std::string &strp : stringpaths) {
        stdfs::path p(strp);
        if (stdfs::exists(p)) {
          if (stdfs::is_regular_file(p)) {
            if (is_file_good(p)) {
              good_stringpaths.push_back(p.string());
            }
          } else if (stdfs::is_directory(p)) {
            if (recursive) {
              //
              // If a directory & user requested recursive
              const std::filesystem::recursive_directory_iterator end;
              for (std::filesystem::recursive_directory_iterator itr{p};
                   itr != end; itr++) {
                if (stdfs::is_regular_file(itr->path())) {
                  if (is_file_good(itr->path())) {
                    good_stringpaths.push_back(itr->path().string());
                  }
                }
              }  // for
            } else {
              //
              // If a directory & user requested recursive
              const std::filesystem::directory_iterator end;
              for (std::filesystem::directory_iterator itr{p}; itr != end;
                   itr++) {
                if (stdfs::is_regular_file(itr->path())) {
                  if (is_file_good(itr->path())) {
                    good_stringpaths.push_back(itr->path().string());
                  }
                }
              }  // for
            }
          }
        }
      }  // for

      //
      // Remove duplicate paths
      std::sort(good_stringpaths.begin(), good_stringpaths.end());
      good_stringpaths.erase(
          std::unique(good_stringpaths.begin(), good_stringpaths.end()),
          good_stringpaths.end());

      // Broadcast paths to all ranks
      m_comm.async_bcast(
          [](auto parquet_reader_ptr, const auto &stringpaths_vec) {
            for (const auto &stringpath : stringpaths_vec) {
              parquet_reader_ptr->m_paths.push_back(stdfs::path(stringpath));
            }
          },
          pthis, good_stringpaths);
    }

    m_comm.barrier();
  }

  /**
   * @brief Checks if file is readable
   *
   * @param p
   * @return true
   * @return false
   */
  bool is_file_good(const stdfs::path &p) {
    std::shared_ptr<arrow::io::ReadableFile> input_file;
    try {
      PARQUET_ASSIGN_OR_THROW(input_file, arrow::io::ReadableFile::Open(p));
    } catch (...) {
      return false;
    }
    return true;
  }

  void read_file_schema() {
    auto reader = open_file(m_paths[0]);
    if (reader == nullptr) {
      throw std::runtime_error("Failed to open the file: " +
                               m_paths[0].string());
    }

    // Get the file schema
    parquet::SchemaDescriptor const *const file_schema =
        reader->metadata()->schema();

    const size_t num_cols = file_schema->num_columns();
    m_col_schema.resize(num_cols);
    for (size_t i = 0; i < num_cols; ++i) {
      // Assumes that Parquet column index space is contiguous, i.e., [0,
      // num_cols).
      parquet::ColumnDescriptor const *const column = file_schema->Column(i);
      auto ptype = detail::parquet_data_type{column->physical_type()};
      // Check if the type is supported
      bool unsupported = !detail::is_supported_parquet_type(ptype.type);
      // Check if the column is flat
      if (column->max_definition_level() != 1 ||
          column->max_repetition_level() != 0) {
        // The column is not flat, which is not supported by this parser.
        unsupported = true;
        // Memo: for definition and repetition levels, see
        // https://blog.x.com/engineering/en_us/a/2013/dremel-made-simple-with-parquet
      }
      m_col_schema[i] = {
          .type = ptype, .name = column->name(), .unsupported = unsupported};
    }
    m_schema_string = file_schema->ToString();
  }

  /// Read multiple parquet files in parallel (MPI)
  /// This function does not read a single file using multiple MPI ranks.
  template <typename Function>
    requires std::invocable<Function, const std::vector<parquet_type_variant> &>
  void read_parquet_files(
      Function                                fn,
      const size_t max_num_rows_to_read,
      std::optional<std::vector<std::string>> columns = std::nullopt) {
    size_t count_rows = 0;
    for (size_t i = 0; i < m_paths.size(); ++i) {
      if (is_owner(i)) {
        assert(max_num_rows_to_read >= count_rows);
        count_rows += read_parquet_file(
            m_paths[i], fn, max_num_rows_to_read - count_rows, columns);
      }
      assert(count_rows <= max_num_rows_to_read);
      if (count_rows >= max_num_rows_to_read) {
        break;
      }
    }
  }

  bool is_owner(const size_t item_ID) {
    return m_comm.rank() == (item_ID % m_comm.size());
  }

  /// Read a parquet file and call a function for each row.
  /// If 'columns' is not empty, only the specified columns are read.
  template <typename Function>
    requires std::invocable<Function, const std::vector<parquet_type_variant> &>
  size_t read_parquet_file(
      const stdfs::path &file_path, Function fn,
      const size_t max_num_rows_to_read,
      std::optional<std::vector<std::string>> columns_to_read = std::nullopt) {
    size_t num_read_rows = 0;
    try {
      // Create a ParquetReader instance
      auto parquet_reader = parquet::ParquetFileReader::OpenFile(file_path);

      // Get the File MetaData
      std::shared_ptr<parquet::FileMetaData> file_metadata =
          parquet_reader->metadata();

      // Get the number of Columns
      const size_t num_columns = file_metadata->num_columns();

      // Find the column indices to read
      // Also, remove unsupported columns from the read list
      std::vector<size_t>     column_indices;
      static constexpr size_t k_invalid_col_index =
          std::numeric_limits<size_t>::max();
      if (columns_to_read) {
        for (const auto &col_name : *columns_to_read) {
          auto it = std::find_if(m_col_schema.begin(), m_col_schema.end(),
                                 [&col_name](const column_schema_type &col) {
                                   return col.name == col_name;
                                 });
          if (it != m_col_schema.end()) {
            const auto col_index = std::distance(m_col_schema.begin(), it);
            // Check if the column is supported
            column_indices.push_back(col_index);
          } else {
            column_indices.push_back(k_invalid_col_index);
          }
        }
      } else {
        // Read all columns. Fill the column indices with the range [0,
        // num_columns)
        column_indices.resize(num_columns);
        std::iota(column_indices.begin(), column_indices.end(), 0);
      }

      // Get the number of RowGroups
      const size_t num_row_groups = file_metadata->num_row_groups();

       // Iterate over all the RowGroups in the file
      for (int r = 0; r < num_row_groups; ++r) {
        std::shared_ptr<parquet::RowGroupReader> row_group_reader =
            parquet_reader->RowGroup(r);

        const auto num_rows = row_group_reader->metadata()->num_rows();

        // Vector of vectors to hold the read values
        // An inner vector holds the values of the same column
        std::vector<std::vector<parquet_type_variant>> read_buf(
            column_indices.size());

        // Read the columns in the RowGroup
        for (int ci = 0; ci < column_indices.size(); ++ci) {
          // Assign std::monostate if A) there is no column associated with the
          // name (invalid index), or B) the column is unsupported.
          if (column_indices[ci] == k_invalid_col_index ||
              m_col_schema[column_indices[ci]].unsupported) {
            read_buf[ci].resize(num_rows, std::monostate{});
            continue;
          }

          std::shared_ptr<parquet::ColumnReader> column_reader =
              row_group_reader->Column(column_indices[ci]);
          auto &col_metadata = m_col_schema.at(column_indices[ci]);
          assert(!col_metadata.unsupported);

          // Read the all column data of the row group
          if (col_metadata.type.equal(parquet::Type::BOOLEAN)) {
            read_buf[ci] =
                typed_row_group_read<parquet::Type::BOOLEAN>(*column_reader);
          } else if (col_metadata.type.equal(parquet::Type::INT32)) {
            read_buf[ci] =
                typed_row_group_read<parquet::Type::INT32>(*column_reader);
          } else if (col_metadata.type.equal(parquet::Type::INT64)) {
            read_buf[ci] =
                typed_row_group_read<parquet::Type::INT64>(*column_reader);
          } else if (col_metadata.type.equal(parquet::Type::FLOAT)) {
            read_buf[ci] =
                typed_row_group_read<parquet::Type::FLOAT>(*column_reader);
          } else if (col_metadata.type.equal(parquet::Type::DOUBLE)) {
            read_buf[ci] =
                typed_row_group_read<parquet::Type::DOUBLE>(*column_reader);
          } else if (col_metadata.type.equal(parquet::Type::BYTE_ARRAY)) {
            read_buf[ci] =
                typed_row_group_read<parquet::Type::BYTE_ARRAY>(*column_reader);
          } else {
            std::cerr << "Unsupported column type: " << col_metadata.type
                      << std::endl;
            throw std::runtime_error("Unsupported Parquet column type");
          }

          // As this parser supports only flat columns,
          // the number of rows read must be equal to the number of rows in
          // the RowGroup.
          if (read_buf[ci].size() != num_rows) {
            std::cerr << "Error reading column " << column_indices[ci] << ": "
                      << read_buf[ci].size() << " rows read, expected "
                      << num_rows << " rows." << std::endl;
            throw std::runtime_error("Error reading Parquet file");
          }
        }

        // Finally, call the user function for each row
        for (size_t i = 0; i < num_rows; ++i) {
          if (num_read_rows == max_num_rows_to_read) {
            return max_num_rows_to_read; // Read enough rows
          }
          assert(num_read_rows < max_num_rows_to_read);

          std::vector<parquet_type_variant> row;
          for (size_t j = 0; j < column_indices.size(); ++j) {
            row.push_back(std::move(read_buf[j][i]));
          }
          fn(row);
          ++num_read_rows;
        }
      }
    } catch (const std::exception &e) {
      // rethrow the exception
      std::cerr << "Error reading Parquet file: " << file_path << " "
                << e.what() << std::endl;
      throw;
    }

    return num_read_rows;
  }

  // Read all values of a row group of a column using
  // parquet::TypedColumnReader. Returns a vector of parquet_type_variant
  template <parquet::Type::type T>
  std::vector<parquet_type_variant> typed_row_group_read(
      parquet::ColumnReader &column_reader) {
    using physical_type_t = parquet::PhysicalType<T>;
    using typed_reader_t  = parquet::TypedColumnReader<physical_type_t>;
    using value_type      = typename physical_type_t::c_type;

    auto *typed_reader = static_cast<typed_reader_t *>(&column_reader);
    std::vector<parquet_type_variant> read_values;
    // Read all the rows in the column
    while (typed_reader->HasNext()) {
      value_type value;
      // Read one value at a time. The number of rows read is returned.
      // values_read contains the number of non-null rows
      int64_t values_read = 0;
      // Memo: Somehow it is needed to get this value from ReadBatch() when
      // reading ByteArray? data If not, the reader fails.
      int16_t    definition_level = 0;
      const auto rows_read        = typed_reader->ReadBatch(
          1, &definition_level, nullptr, &value, &values_read);

      // Ensure only one value is read
      if (rows_read != 1) {
        std::cerr << "Error: read " << rows_read
                  << " rows (expected to read only one row)." << std::endl;
        std::abort();
      }

      if (values_read == 0) {
        // Null value
        read_values.emplace_back(std::monostate{});
      } else {
        if constexpr (std::is_same_v<value_type, parquet::ByteArray>) {
          // Convert to std::string_view first as ByteArray has a conversion
          // operator to std::string_view
          read_values.emplace_back(
              std::string(static_cast<std::string_view>(value)));
        } else {
          read_values.push_back(value);
        }
      }
    }

    return read_values;
  }

  ygm::comm                      &m_comm;
  ygm::ygm_ptr<self_type>         pthis;
  std::vector<stdfs::path>        m_paths;
  std::vector<column_schema_type> m_col_schema;
  std::string                     m_schema_string;
  size_t                          m_num_total_rows{0};
};
}  // namespace ygm::io
