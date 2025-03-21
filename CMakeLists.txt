# Copyright 2019-2021 Lawrence Livermore National Security, LLC and other YGM
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

# ---------------------------------------------------------------------------- #
# YGM configuration
# ---------------------------------------------------------------------------- #
project(
    ygm
    VERSION 0.8
    DESCRIPTION "HPC Communication Library"
    LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

#
# Configuration options for YGM
#
option(YGM_BUILD_TESTS "Build tests" OFF)
option(YGM_BUILD_EXAMPLES "Build examples" OFF)
option(YGM_BUILD_TOOLS "Build tools" OFF)
option(YGM_DOXYGEN "Adds targets for generating Doxygen documentation" OFF)
option(YGM_RTD_ONLY
       "Run Cmake for only generating documentation for Read the Docs" OFF
)
option(YGM_REQUIRE_PARQUET "YGM requires Arrow Parquet." OFF)
option(YGM_INSTALL_PARQUET "YGM installs Arrow Parquet if it is not found." OFF)

#
# Doxygen and Read the Docs (RTD) Documentation
#
if (YGM_DOXYGEN OR YGM_RTD_ONLY)
    add_subdirectory(docs)
    if (YGM_RTD_ONLY)
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

#
# Cereal
#
if (NOT cereal_POPULATED)
    find_package(cereal CONFIG QUIET)
    if (NOT cereal_FOUND)
        set(JUST_INSTALL_CEREAL TRUE)
        FetchContent_Declare(
            cereal
            GIT_REPOSITORY https://github.com/USCiLab/cereal.git
            GIT_TAG v1.3.2
        )
        FetchContent_MakeAvailable(cereal)
    endif ()
endif ()

#
# Boost
#
if (NOT boost_POPULATED)
    find_package(Boost 1.78)
    if (Boost_FOUND)
        message(STATUS ${PROJECT_NAME} " found boost include dirs: "
                       ${Boost_INCLUDE_DIRS}
        )
    else ()
        FetchContent_Declare(
            Boost
            URL https://github.com/boostorg/boost/releases/download/boost-1.87.0/boost-1.87.0-cmake.tar.gz
        )
        set(BOOST_INCLUDE_LIBRARIES json)
        FetchContent_MakeAvailable(Boost)
    endif ()
endif ()

#
# Arrow
#
include(FindArrowParquet)
find_or_install_arrow_parquet()

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
    INTERFACE MPI::MPI_CXX Threads::Threads stdc++fs cereal
)
if (UNIX AND NOT APPLE)
    target_link_libraries(
        ygm
        PUBLIC
        INTERFACE rt
    )
endif ()

# Install ygm. Expects CMAKE_INSTALL_PREFIX to be set to a suitable directory.
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(
    TARGETS ${PROJECT_NAME} cereal
    EXPORT ${PROJECT_NAME}Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    EXPORT ${PROJECT_NAME}Targets
    FILE ${PROJECT_NAME}Targets.cmake
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake
)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/ygm
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# create version file
write_basic_package_version_file(
    "${PROJECT_NAME}ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion
)

# create config file
configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in"
    "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake
)

install(FILES "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
              "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake
)

if (YGM_BUILD_TESTS)
    option(TEST_WITH_SLURM "Run tests with Slurm" OFF)
    enable_testing()
    add_subdirectory(test)
endif ()
if (YGM_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()
if (YGM_BUILD_TOOLS)
    add_subdirectory(tools)
endif ()
