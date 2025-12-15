# Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
# Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: MIT

#
# This function adds a Sequential test.
#
function (add_ygm_seq_test test_name)
    set(test_source "${test_name}.cpp")
    set(test_exe "SEQ_${test_name}")
    add_executable(${test_exe} ${test_source})
    target_compile_options(${test_exe} PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(${test_exe} PUBLIC ygm::ygm)
    add_test(${test_exe} "${CMAKE_CURRENT_BINARY_DIR}/${test_exe}")
endfunction ()

#
# This function adds an MPI test.
#
function (add_ygm_test test_name)
    set(test_source "${test_name}.cpp")
    set(test_exe "MPI_${test_name}")
    add_executable(${test_exe} ${test_source})
    target_compile_options(${test_exe} PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(${test_exe} PUBLIC ygm::ygm)
    if (${TEST_WITH_SLURM})
        add_test(${test_exe} "srun" "${CMAKE_CURRENT_BINARY_DIR}/${test_exe}")
    else ()
        add_test(${test_exe} ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} "4"
                 ${MPIEXEC_PREFLAGS} "${CMAKE_CURRENT_BINARY_DIR}/${test_exe}"
        )
    endif ()
endfunction ()

add_ygm_seq_test(test_cereal_archive)
add_ygm_seq_test(test_byte_vector)
add_ygm_seq_test(test_logger)
add_ygm_seq_test(test_cereal_boost_json)
add_ygm_seq_test(test_cereal_boost_container)

add_ygm_test(test_comm)
add_ygm_test(test_comm_2)
add_ygm_test(test_barrier)
add_ygm_test(test_lambda_capture)
add_ygm_test(test_layout)
add_ygm_test(test_large_messages)
add_ygm_test(test_map)
add_ygm_test(test_set)
add_ygm_test(test_bag)
#add_ygm_test(test_tagged_bag)
add_ygm_test(test_array)
#add_ygm_test(test_counting_set)
add_ygm_test(test_disjoint_set)
#add_ygm_test(test_container_serialization)
add_ygm_test(test_line_parser)
add_ygm_test(test_csv_parser)
add_ygm_test(test_csv_headers)
add_ygm_test(test_multi_output)
add_ygm_test(test_daily_output)
add_ygm_test(test_interrupt_mask)
#add_ygm_test(test_reducing_adapter)
add_ygm_test(test_random)
#add_ygm_test(test_reduce_by_key)
add_ygm_test(test_container_traits)
add_ygm_test(test_collective)
add_ygm_test(test_traits)
add_ygm_test(test_concepts)
add_ygm_test(test_recursion_large_messages)
add_ygm_test(test_recursion_progress)
add_ygm_test(test_gather_topk)
add_ygm_test(test_reduce)
add_ygm_test(test_transform)
add_ygm_test(test_local_visit)
add_ygm_test(test_progress_indicator)
add_ygm_test(test_boost_string)
add_ygm_test(test_wout_werr)
add_ygm_test(test_ndjson_parser)
add_ygm_test(test_stats)
add_ygm_test(test_work_queue)


if (Arrow_FOUND AND Parquet_FOUND)
    add_ygm_test(test_parquet_reader)
    link_arrow_parquet(MPI_test_parquet_reader)

    add_library(test_multi_compilation_unit_lib test_multi_compilation_unit_lib.cpp)
    target_link_libraries(test_multi_compilation_unit_lib PUBLIC ygm::ygm)
    link_arrow_parquet(test_multi_compilation_unit_lib)
    add_ygm_test(test_multi_compilation_unit_driver)
    target_link_libraries(MPI_test_multi_compilation_unit_driver PRIVATE test_multi_compilation_unit_lib)
    link_arrow_parquet(MPI_test_multi_compilation_unit_driver)
endif ()

#
# Copy the testing data directory
#
file(COPY data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
