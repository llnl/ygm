.. _ygm-io:

*********************************
:code:`ygm::io` module reference
*********************************

The ``ygm::io`` module provides parallel I/O functionality for use with YGM's communicator. This allows for simple
parallel reading of large (collections of) files where each line can be read independently of all others and writing of
output to collections of files.

Reading Input
=============

The reading functionality of YGM is built around the ``ygm::io::line_parser`` object. 
The ``for_all`` method of the line parser takes a lambda that
is executed on every line of text within the files. As an example, the following code will read through ``file1.txt`` and
``file2.txt`` and count the lines that contain more than 10 characters:

.. code-block:: C++

  ygm::io::line_parser my_line_parser(world, {"file1.txt", "file2.txt"});

  int long_line_count{0};
  my_line_parser.for_all([&long_line_count](const std::string &line) {
    if (line.size() > 10) {
      ++long_line_count;
    });

  long_line_count = ygm::sum(long_line_count, world);

The line parser assigns contiguous chunks of the files being read to all ranks in the communicator (with a minimum size
to avoid partitioning files into unrealistically small pieces). This splitting is done based on the number of bytes
within files, with starting positions adjusted to the nearest newline. For this reason, it must be possible to process
each line of the input files independently of all others, and there is not support for more complicated record parsing.

YGM has parsers (often built on top of the ``ygm::io::line_parser``) for when data is provided in specific formats. These
function in much the same way as the ``line_parser``, but do not require as much manual parsing of individual lines.

CSV Parser
----------

The ``ygm::io::csv_parser`` takes each line of input and parses it into a ``csv_line`` object before it is provided to the
user's lambda in a ``for_all`` call. This parsing converts all comma-separated values within a line into positional
arguments that can be accessed from the ``csv_line`` and converted into various types. As an example, the following code
reads all values within a line, checks to make sure they are usable as doubles, converts them to doubles, adds them up, and prints the result.
This example also sums up the final entry in each column:

.. code-block:: C++

   ygm::io::csv_parser my_csv_parser(world, {"file1.csv", "file2.csv"});

   double final_sum{0.0};
   my_csv_parser.for_all([&final_sum](const auto &line_csv) {
      double line_total;
      for (auto &entry : line_csv) {
        assert(entry.is_double());
        line_total += entry.as_double();
      }
      final_sum += line_csv[line_csv.size()-1];
      std::cout << "Line total: " << line_total << std::endl;
    });

    final_sum = ygm::sum(final_sum, world);

The entries within a parsed CSV line are stored as ``csv_field`` types. The following shows all of the available methods
for checking the types of fields and converting them to primitive types:

.. doxygenclass:: ygm::io::detail::csv_field
   :members:
   :undoc-members:

CSVs with Headers
^^^^^^^^^^^^^^^^^

Many CSV files contain header lines that provide meaningful names to the columns of a file. For cases like these, the
``ygm::io::csv_parser`` has a ``read_headers`` method that reads the first line of the CSV files as a collection of column
headers and provides named access to the columns in subsequent ``for_all`` calls. For example, we can sum values in
``important_column1`` and ``important_column2`` in CSV files containing columns named ``important_column1``, ``other_column``,
and ``important_column2`` as follows:

.. code-block:: C++

   ygm::io::csv_parser my_csv_parser(world, {"file1.csv", "file2.csv", "file3.csv"});
   my_csv_parser.read_headers();

   double important_sum{0.0};
   my_csv_parser.for_all([&important_sum](const auto &line_csv) {
    important_sum += line_csv["important_column1"].as_double();
    important_sum += line_csv["important_column2"].as_double();
  });

When reading CSV files with headers, it is important to remember that
  * all CSV files provided must contain headers that are identical
  * if a CSV file with headers is read without calling ``read_headers()`` the header line will be treated as a normal line with data

NDJSON Parser
-------------

The ``ygm::io::ndjson_parser`` handles lines of input that are provided as newline-delimited JSON (NDJSON), a.k.a. JSON
lines data. JSON support is provided by `Boost JSON`_ and requires some knowledge of the associated syntax. To sum
the ``number`` field in all JSON records as integers, we can do the following:

.. code-block:: C++

   ygm::io::ndjson_parser my_json_parser(world, {"file1.ndjson"});

   int64_t total{0};
   my_ndjson_parser.for_all([&total](const auto &json_line) {
    if (json_line["number"].is_int64()) {
      json_line["number"].as_int64();
    }
   });

   total = ygm::sum(total, world);

.. _Boost JSON: https://www.boost.org/doc/libs/latest/libs/json/doc/html/index.html

Parquet Parser
--------------

YGM provides Parquet parsing through the use of `Apache Arrow`_ in its ``ygm::io::parquet_parser``. A row of data is
provided to a ``for_all`` operation as a ``vector`` of data entries provided as a ``variant``. Optionally, a ``vector`` of column names can be provided
as to specify the set of columns needed by the lambda being executed on the rows. If no columns names are provided, the
default behavior is to provide all columns to the lambda. To print the "string_column" and "float_column" columns of a
Parquet dataset, use:

.. code-block:: C++

   ygm::io::parquet_parser my_parquet_parser(world, {"file.parquet"});

   my_parquet_parser.for_all({"string_column", "float_column"}(const auto &row_values) {
    std::cout << std::get<std::string>(row_values[0]) << "\t" << std::get<float>(row_values[1]) << std::endl;
   });

.. _Apache Arrow: https://arrow.apache.org/

Writing Output
==============

When writing large amounts of output, there are two main ways of doing so in YGM. The simplest and most frequently
encountered is where output is written in a manner that does not require organization. In these situations, it is
easiest to have every rank open a separate file (using ``std::ofstream``) that is distinct from files on all other ranks
for writing its own local data.

The second supported way of writing files is when output generated anywhere on the system has a natural filename that it
must be found in. In this case, independent ranks cannot open all files and write to them safely. For such use cases,
YGM provides the ``ygm::io::multi_output``. This object takes a filename that a line of output must be written to and
communicates the line to a specific rank that is responsible for writing to that filename.

An example of doing so is:

.. code-block:: C++

  std::string output_directory{"output_dir/"};
  ygm::io::multi_output mo(world, output_directory);

  mo.async_write_line("file1", 14);
  mo.async_write_line("file2", "this is some output");

One use case of this functionality is when each line of output has a timestamp, and the output lines need to be
organized by the day associated with their timestamp. This behavior is provided by the ``ygm::io::daily_output``, which
acts the same as the `multi_output`, but all calls to ``async_write_line`` take a timestamp as the number of seconds since
the Unix epoch instead of a filename. Files are then written to in a directory format of ``year/month/day`` within the
output directory passed to the ``ygm::io::daily_output`` constructor.

.. toctree::
   :maxdepth: 1
   :caption: I/O Classes:

   io/line_parser
   io/csv_parser
   io/ndjson_parser
   io/parquet_parser
   io/multi_output
   io/daily_output
