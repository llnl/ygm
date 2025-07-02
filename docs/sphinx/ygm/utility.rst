.. _ygm-utility:

*************************************
:code:`ygm::utility` module reference
*************************************

The utility namespace contains multiple components that often helpful when using YGM, but may not be necessary to use.
Their uses include tracking the performance of YGM, getting easy access to basic functionality built on
`MPI_COMM_WORLD`, and sending messages using YGM that contain specialized data types. The headers containing this
functionality can be safely included in user programs, but is often already included in other YGM headers because they
can be used within YGM.

:code:`ygm::utility::timer`
=====================

The `ygm::utility::timer` class starts a very simple timer using `MPI_Wtime`. It includes `elapsed()` and `reset()`
methods for checking the time since the timer has been started and resetting the start time of a timer, respectively.

Typical use of the `ygm::utility::timer` is:

.. code-block:: C++

  ygm::utility::timer t{};
  {
    // Do stuff
  }
  world.barrier();
  world.cout0("Time: ", t.elapsed());

``ygm::utility::progress_indicator``
==================================

The `ygm::utility::progress_indicator` asynchronously tracks progress through a calculation across all processes, with
each process periodically sending updates that are printed by rank 0. The `progress_indicator` prints the total number
of work items completed and the rate at which they are completing.

The `async_inc()` method of the `progress_indicator` is used to locally indicate when work is progressing. This call
will begin a nonblocking reduction when enough work has been completed to collect the output for printing. 
An internal `progress_indicator::options` class is used to control the message for printing and the frequency with which
reductions and printing occurs.

Typical use of the `ygm::utility::timer` is:

.. code-block:: C++

   ygm::utilijty::progress_indicator prog(world, {.update_freq = 10, .message = "Doing stuff"});
   for (int i=0; i<1000; ++i) {
    prog.async_inc();
    // Do work
   }
   prog.complete();
   world.barrier();

Global World Functionality
==========================

YGM provides the following functions for basic interactions with `MPI_COMM_WORLD` that can be done from anywhere within
a YGM program:

* `ygm::wrank()` - returns the current process's rank
* `ygm::wrank0()` - returns a boolean indicating whether the current rank is rank 0 or not
* `ygm::wsize()` - returns the number of ranks in `MPI_COMM_WORLD`
* `ygm::wcout0()` - prints output to `std::cout` from only rank 0
* `ygm::wcerr0()` - same as `ygm::wcout0()` but provides `std::cerr` access for just rank 0
* `ygm::wcout()` - prints output to `std::cout` from the current rank with its rank prepended
* `ygm::wcerr()` - same as `ygm::wcout()` but prints to `std::cerr`

The printing functionality can be used either to get access to an output stream for printing or as a `print()` type of
function, that is `ygm::wcout0() << "Printint output"` and `ygm::wcout0("Printing output")` will produce the same output.

Asserts
=======

`assert.hpp` provides a small number of assert macros:

* `YGM_ASSERT_MPI` - used for wrapping MPI calls to detect when MPI does not return `MPI_SUCCESS`
* `YGM_ASSERT_DEBUG` - same functionality as `assert`
* `YGM_ASSERT_RELEASE` - assert statement that is triggered even if `NDEBUG` is defined

Specialized Serialization Functions
=======================================

A number of headers are provided for serialization of datatypes for communication through YGM:

* `boost_*.hpp` - serialization for various Boost types

Utility Class Documentation
===========================

.. toctree::
   :maxdepth: 1
   :caption: Utility Classes:

   utility/timer
   utility/progress_indicator
