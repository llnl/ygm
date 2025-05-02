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
    add_ygm_tool(parquet_tools)
    link_arrow_parquet(parquet_tools)

    # Change the way to link Boost based on how Boost is found
    if (Boost_FOUND)
        # Found by find_package()
        target_link_libraries(parquet_tools PUBLIC ${Boost_INCLUDE_DIRS})
    elseif (boost_POPULATED)
        # Found by FetchContent
        target_link_libraries(parquet_tools PUBLIC Boost::json)
    endif()
endif ()
file(COPY ./parquet_tools_subcmd.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR})