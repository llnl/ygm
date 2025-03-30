# Copyright 2019-2023 Lawrence Livermore National Security, LLC and other YGM
# Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: MIT

if (Arrow_FOUND AND Parquet_FOUND)
    add_ygm_example(parquet_reader)
    link_arrow_parquet(parquet_reader)
else ()
    message(STATUS "Arrow or Parquet not found, skipping bulding arrow_parquet examples.")
endif ()