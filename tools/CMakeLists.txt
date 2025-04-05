# Copyright 2019-2024 Lawrence Livermore National Security, LLC and other YGM
# Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: MIT

if (Arrow_FOUND AND Parquet_FOUND)
    if (Boost_FOUND OR boost_POPULATED)
        add_ygm_example(parquet_tools)
        link_arrow_parquet(parquet_tools)
        if (Boost_FOUND)
            target_include_directories(parquet_tools PRIVATE ${Boost_INCLUDE_DIRS})
        else (boost_POPULATED)
            target_link_libraries(parquet_tools PRIVATE Boost::json)
        endif ()
    endif ()
endif ()
file(COPY ./parquet_tools_subcmd.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR})