# Copyright 2019-2024 Lawrence Livermore National Security, LLC and other YGM
# Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: MIT

#
# This function adds a tool
#
function (add_ygm_tool tool_name)
    set(tool_source "${tool_name}.cpp")
    set(tool_exe "${tool_name}")
    add_executable(${tool_exe} ${tool_source})
    target_link_libraries(${tool_exe} PUBLIC ygm::ygm)
endfunction ()

if (Boost_FOUND)
add_ygm_tool(read_trace)
target_include_directories(read_trace PUBLIC ${Boost_INCLUDE_DIRS})
endif ()


if (Arrow_FOUND AND Parquet_FOUND)
    if (Boost_FOUND OR boost_POPULATED)
        add_ygm_example(parquet_tools)
        target_include_directories(parquet_tools PUBLIC ${Boost_INCLUDE_DIRS})
        link_arrow_parquet(parquet_tools)
        if (Boost_FOUND)
            target_include_directories(parquet_tools PRIVATE ${Boost_INCLUDE_DIRS})
        else (boost_POPULATED)
            target_link_libraries(parquet_tools PRIVATE Boost::json)
        endif ()
    endif ()
endif ()
file(COPY ./parquet_tools_subcmd.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR})