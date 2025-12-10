Getting Started
***************

What is YGM?
============

YGM is an asynchronous communication library written in C++ and designed for high-performance computing (HPC) use cases featuring 
irregular communication patterns. YGM includes a collection of
distributed-memory storage containers designed to express common algorithmic and data-munging tasks. These containers
automatically partition data, allowing insertions and, with most containers, processing of individual elements to be
initiated from any running YGM process.

Underlying YGM's containers is a communicator abstraction. This communicator asynchronously sends messages spawned by
senders with receivers needing no knowledge of incoming messages prior to their arrival. YGM communications take the
form of *active messages*; each message contains a function object to execute (often in the form of C++ lambdas), data
and/or pointers to data for this function to execute on, and a destination process for the message to be executed at.

YGM also includes a set of I/O primitives for parsing collections of input documents in parallel as independent lines of
text and streaming output lines to
large numbers of destination files. Current parsing functionality supports reading input as CSV, ndjson, and
unstructured lines of data.

General YGM Operations
======================

YGM is built on its ability to communicate active messages asynchronously between running processes. This does not
capture every operation that can be useful, for instance collective operations are still widely needed. YGM uses
prefixes on function names to distinguish their behaviors in terms of the processes involved. These prefixes are:
   * ``async_``: Asynchronous operation initiated on a single process. The execution of the underlying function may
     occur on a remote process.
   * ``local_``: Function performs only local operations on data of the current process. In uses within YGM containers
     with partitioning schemes that determine item ownership, care must be taken to ensure the process a ``local_``
     operation is called from aligns with the item's owner. For instance, calling ``ygm::container::map::local_insert``
     will store an item on the process where the call is made, but the ``ygm::container::map`` may not be able to look
     up this location if it is on the wrong process.
   * No Prefix: Collective operation that must be called from all processes.

The primary workhorse functions in YGM fall into the two categories of ``async_`` and ``for_all`` operations. In an
``async_`` operation, a lambda is asynchronously sent to a (potentially) remote process for execution. In many cases
with YGM containers, the lambda being executed is not provided by the user and is instead part of the function itself,
e.g. ``async_insert`` calls on most containers. A ``for_all`` operation is a collective operation in which a lambda is 
executed locally on every process while iterating over all locally held items of some YGM object. The items iterated
over can be items in a YGM container, items coming from a map, filter, or flatten applied to a container, or all lines
in a collection of files in a YGM I/O parser.

Lambda Capture Rules
--------------------
Certain ``async_`` and ``for_all`` operations require users to provide lambdas as part of their executions. The lambdas
that can be accepted by these two classes of functions follow different rules pertaining to the capturing of variables:
   * ``async_`` calls cannot capture (most) variables in lambdas. Variables necessary for lambda execution must be
     provided as arguments to the ``async_`` call. In the event that the data for the lambda resides on the remote
     process the lambda will execute on, a ``ygm::ygm_ptr`` should be passed as an argument to the ``async_``.
   * ``for_all`` calls assume lambdas take only the arguments inherently provided by the YGM object being iterated over.
     All other necessary variables *must* be captured. The types of arguments provided to the lambda can be identified
     by the ``for_all_args`` type within the YGM object.

These differences in behavior arise from the distinction that ``async_`` lambdas may execute on a remote process, while
``for_all`` lambdas are guaranteed to execute locally to a process. In the case of ``async_`` operations, the lambda and
all arguments must be serialized for communication, but C++ does not provide a method for inspection of variables
captured in the closure of a lambda. In the case of ``for_all`` operations, the execution is equivalent to calling
`std::for_each <https://en.cppreference.com/w/cpp/algorithm/for_each>`_ on entire collection of items held locally.

Requirements
============

* C++20 - GCC versions 11 and 12 are tested. Your mileage may vary with other compilers.
* `Cereal <https://github.com/USCiLab/cereal>`_ - C++ serialization library
* MPI
* Optionally, Boost 1.77 to enable Boost.JSON support.  


Using YGM with CMake
====================
YGM is a header-only library that is easy to incorporate into a project through CMake. Adding the following to
CMakeLists.txt will install YGM and its dependencies as part of your project:

.. code-block:: CMake

   set(DESIRED_YGM_VERSION 0.6)
   find_package(ygm ${DESIRED_YGM_VERSION} CONFIG)
   if (NOT ygm_FOUND)
       FetchContent_Declare(
           ygm
           GIT_REPOSITORY https://github.com/LLNL/ygm
           GIT_TAG v${DESIRED_YGM_VERSION}
       )
       FetchContent_GetProperties(ygm)
       if (ygm_POPULATED)
           message(STATUS "Found already populated ygm dependency: "
                          ${ygm_SOURCE_DIR}
           )
       else ()
           set(JUST_INSTALL_YGM ON)
           set(YGM_INSTALL ON)
           FetchContent_Populate(ygm)
           add_subdirectory(${ygm_SOURCE_DIR} ${ygm_BINARY_DIR})
           message(STATUS "Cloned ygm dependency " ${ygm_SOURCE_DIR})
       endif ()
   else ()
       message(STATUS "Found installed ygm dependency " ${ygm_DIR})
   endif ()

License
=======
YGM is distributed under the MIT license.

All new contributions must be made under the MIT license.

See `LICENSE-MIT <https://github.com/LLNL/ygm/blob/master/LICENSE-MIT>`_, `NOTICE
<https://github.com/LLNL/ygm/blob/master/NOTICE>`_, and `COPYRIGHT <https://github.com/LLNL/ygm/blob/master/COPYRIGHT>`_ for
details.

SPDX-License-Identifier: MIT

Release
=======
LLNL-CODE-789122
