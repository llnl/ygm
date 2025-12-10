# Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
# Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.14)

# ---------------------------------------------------------------------------- #
# CMake policy
# ---------------------------------------------------------------------------- #
if (${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.30")
    cmake_policy(SET CMP0167 NEW)
endif ()

if (${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.24")
    cmake_policy(SET CMP0135 NEW)
endif ()

# ---------------------------------------------------------------------------- #
# YGM configuration
# ---------------------------------------------------------------------------- #
project(
    ygm
    VERSION 0.9
    DESCRIPTION "HPC Communication Library"
    LANGUAGES CXX
)

#
# Configuration options for YGM
#
option(YGM_BUILD_TESTS "Build tests" OFF)
option(YGM_BUILD_DOCS "Adds targets for generating Doxygen and Sphinx documentation" OFF)
option(YGM_DOCS_ONLY "Run Cmake for only generating documentation (i.e. for Read the Docs)" OFF)
option(YGM_INSTALL_PARQUET "Installs Arrow Parquet." OFF)
set(YGM_PYTHON_VENV_ROOT CACHE PATH "Path to a Python virtual environment root. If set, YGM will use this virtual environment for Python dependencies.")
option(YGM_DONT_USE_MREMAP "Do not try using mremap for managing memory." OFF)

#
# FetchContent dependencies that can be overridden on the command line.
#
set(BOOST_FETCH_URL 
    "https://github.com/boostorg/boost/releases/download/boost-1.87.0/boost-1.87.0-cmake.tar.gz" 
    CACHE STRING "URL to fetch Boost tarball")
set(CEREAL_FETCH_GIT 
    "https://github.com/USCiLab/cereal.git" 
    CACHE STRING "GIT to fetch Cereal")
set(SPDLOG_FETCH_GIT 
    "https://github.com/gabime/spdlog.git" 
    CACHE STRING "GIT to fetch Spdlog")


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

#
# Doxygen and Read the Docs (RTD) Documentation
#
if (YGM_BUILD_DOCS OR YGM_DOCS_ONLY)
    add_subdirectory(docs)
    if (YGM_DOCS_ONLY)
        return()
    endif ()
endif ()

# Require out-of-source builds
file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/CMakeLists.txt" LOC_PATH)
if (EXISTS "${LOC_PATH}")
    message(
        FATAL_ERROR
            "You cannot build in a source directory (or any directory with a CMakeLists.txt file). Please make a build subdirectory. Feel free to remove CMakeCache.txt and CMakeFiles."
    )
endif ()

#
# MPI and Threads are required
#
find_package(MPI REQUIRED)
find_package(Threads REQUIRED)

# prepare to FetchContent
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

#
# Cereal
#
if (NOT cereal_POPULATED)
    find_package(cereal CONFIG QUIET)
    if (NOT cereal_FOUND)
        set(JUST_INSTALL_CEREAL TRUE)
        FetchContent_Declare(
            cereal
            GIT_REPOSITORY ${CEREAL_FETCH_GIT}
            GIT_TAG v1.3.2
        )
        FetchContent_MakeAvailable(cereal)
    endif ()
endif ()

#
# Boost
#
if (NOT boost_POPULATED)
    message(STATUS "Fetching Boost from source")
    FetchContent_Declare(
        Boost
        URL ${BOOST_FETCH_URL}
    )
    set(BOOST_INCLUDE_LIBRARIES json unordered container static_string)
    FetchContent_MakeAvailable(Boost)
endif ()

#
# spdlog
#
if (NOT spdlog_POPULATED)
    set(SPDLOG_INSTALL On)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY ${SPDLOG_FETCH_GIT}
        GIT_TAG v1.14.1
    )
    FetchContent_MakeAvailable(spdlog)
endif ()

#
# Arrow
#
include(FindArrowParquet)
if (YGM_INSTALL_PARQUET)
    install_arrow_parquet()
endif ()

#
# Create the YGM target library
#
add_library(ygm INTERFACE)
add_library(ygm::ygm ALIAS ygm)
target_compile_features(ygm INTERFACE cxx_std_20)
target_include_directories(
    ygm INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                  $<INSTALL_INTERFACE:include>
)
target_link_libraries(
    ygm
    PUBLIC
    INTERFACE MPI::MPI_CXX Threads::Threads cereal spdlog_header_only Boost::container Boost::unordered Boost::static_string Boost::json
)

if (YGM_DONT_USE_MREMAP)
    target_compile_definitions(ygm INTERFACE YGM_DONT_USE_MREMAP)
endif ()

if (UNIX AND NOT APPLE)
    target_link_libraries(
        ygm
        PUBLIC
        INTERFACE rt
    )
endif ()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_link_libraries(ygm INTERFACE stdc++fs)
endif()

if (YGM_BUILD_TESTS)
    option(TEST_WITH_SLURM "Run tests with Slurm" OFF)
    enable_testing()
    add_subdirectory(test)
endif ()
