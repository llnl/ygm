# Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
# Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: MIT

#
# This function adds an mpi example.
#
function (add_ygm_example example_name)
    set(example_source "${example_name}.cpp")
    set(example_exe "${example_name}")
    add_executable(${example_exe} ${example_source})
    target_link_libraries(${example_exe} PUBLIC ygm::ygm Boost::unordered Boost::json)
endfunction ()

add_ygm_example(howdy_world)
add_ygm_example(howdy_world_recursive)
add_ygm_example(lambda_optional_arguments)

add_subdirectory(container)
add_subdirectory(io)
