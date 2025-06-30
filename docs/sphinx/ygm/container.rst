.. _ygm-container:

****************************************
:code:`ygm::container` module reference.
****************************************

:code:`ygm::container` is a collection of distributed containers designed specifically
to perform well within YGM's asynchronous runtime.
Inspired by C++'s Standard Template Library (STL), the containers provide
improved programmability by allowing developers to consider an algorithm as the
operations that need to be performed on the data stored in a container while
abstracting the locality and access details of said data.
While insiration is taken from STL, the top priority is to provide expressive
and performant tools within the YGM framework.

Implemented Storage Containers
==============================

The currently implemented containers include a mix of distributed versions of familiar containers and
distributed-specific containers:

   * ``ygm::container::bag`` - An unordered collection of objects partitioned across processes. Ideally suited for
     iteration over all items with no capability for identifying or searching for an individual item within the bag.
   * ``ygm::container::set`` - Analogous to ``std::set``. An unordered collection of unique objects with the ability to iterate
     and search for individual items. Insertion and iteration are slower than a ``ygm::container::bag``.
   * ``ygm::container::map`` - Analogous to ``std::map``. A collection of keys with assigned values. Keys and values can
     be inserted and looked up individually or iterated over collectively.
   * ``ygm::container::array`` - A collection of items indexed by an integer type. Items can be inserted and looked up
     by their index values independently or iterated over collectively. Differs from a ``std::array`` in that sizes do
     not need to known at compile-time, and a ``ygm::container::array`` can be dynamically resized through a
     (potentially expensive) function at runtime.
   * ``ygm::container::counting_set`` - A container for counting occurrences of items. Can be thought of as a
     ``ygm::container::map`` that maps items to integer counts but optimized for the case of frequent duplication of
     keys.
   * ``ygm::container::disjoint_set`` - A distributed disjoint set data structure. Implements asynchronous union
     operation for maintaining membership of items within mathematical disjoint sets. Eschews the find operation of most
     disjoint set data structures and instead allows for execution of user-provided lambdas upon successful completion
     of set merges.

Typical Container Operations
============================

Most interaction with containers occurs in one of two classes of operations:
iteration and ``async_``.

Iterating over Containers
-------------------------

Elements within a container can be iterated over using calls to ``for_all`` methods or using standard C++ iterators. In
their standard form, both iteration techniques make calls to a YGM ``barrier`` on the underlying communicator to ensure
that all updates to the container have been processed before starting the iteration. ``local_`` variants for both exist
that skip the call to ``barrier``, allowing them to be called in a non-collective context.

Container :code:`for_all` Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^

:code:`for_all`-class operations are barrier-inducing collectives that direct
ranks to iteratively apply a user-provided function to all locally-held data.
Functions passed to the :code:`for_all` interface do not support additional
variadic parameters. However, these functions are stored and executed locally on each rank, and so
can capture objects in rank-local scope. The ``local_for_all`` variant has the same API as ``for_all``, but skips the
internal call to ``barrier`` at its beginning.

The following example shows a `for_all` being used to double all values in a ``ygm::container::bag<int>`` called ``my_bag``:

.. code-block:: C++

   int multiple{2};

   my_bag.for_all([&multiple](int &value) {
    value = value * multiple;
    });

The above example uses a capture of the ``multiple`` variable that can be used within the lambda executed on each value
within the bag.

Container Iterators
^^^^^^^^^^^^^^^^^^^

Iteration can also be performed using iterators. The ``begin()`` and ``end()`` methods return iterators to the local data
stored within a rank. This allows for range-based for loops that have more control over the flow of the loop. For
instance, this example adds all values within a ``ygm::container::bag<int>`` named ``my_bag`` until the first odd value is
encountered:

.. code-block:: C++

   int even_sum{0};

   for (const auto &value : my_bag) {
    if (value % 2 == 1) {
      break;
    }

    even_sum += value;
   }

When using iterators to YGM containers, it is important to remember that ``begin()`` and ``end()`` are collective calls that
include a ``barrier`` to make sure all updates to the container have been processed. This can easily lead to deadlocks if
not used carefully. The ``local_begin()`` and ``local_end()`` calls return the same iterators to the data within a rank as
``begin()`` and ``end()`` but do not call ``barrier`` at the beginning. These can be used to iterate locally within a single
rank with the understanding that there may be messages queued that attempt to update values within the container which
may need to be considered.

:code:`async_` Operations
-------------------------

Operations prefixed with ``async_`` perform operations on containers that can be spawned from any process and
execute on the correct process using YGM's asynchronous runtime. The most common ``async`` operations are:

   * ``async_insert`` - Inserts an item or a key and value, depending on the container being used. The process responsible
     for storing the inserted object is determined using the container's partitioner. Depending on the container, this
     partitioner may determine this location using a hash of the item or by heuristics that attempt to evenly spread
     data across processes (in the case of ``ygm::container::bag``).
   * ``async_visit`` - Items within YGM containers will be distributed across the universe of running processes. Instead
     of providing operations to look up this data directly, which would involve a round-trip communication with the
     process storing the item of interest, most YGM containers provide ``async_visit``. A call to ``async_visit`` takes
     a function to execute and arguments to pass to the function and asynchronously executes the provided function with
     arguments that are the item stored in the container and the additional arguments passed to ``async_visit``.

Specific containers may have additional ``async_`` operations (or may be missing some of the above) based on the
capabilities of the container. Consult the documentation of individual containers for more details.

YGM Container Example
=====================

.. code-block:: C++

  #include <ygm/comm.hpp>
  #include <ygm/container/map.hpp>
  
  int main(int argc, char **argv) {
    ygm::comm world(&argc, &argv);
  
    ygm::container::map<std::string, std::string> my_map(world);
  
    if (world.rank0()) {
      my_map.async_insert("dog", "bark");
      my_map.async_insert("cat", "meow");
    }
  
    world.barrier();
  
    auto favorites_lambda = [](auto key, auto &value, const int favorite_num) {
      std::cout << "My favorite animal is a " << key << ". It says '" << value
                << "!' My favorite number is " << favorite_num << std::endl;
    };
  
    // Send visitors to map
    if (world.rank() % 2) {
      my_map.async_visit("dog", favorites_lambda, world.rank());
    } else {
      my_map.async_visit("cat", favorites_lambda, world.rank() + 1000);
    }
  
    return 0;
  }

Container Transformation Objects
================================

``ygm::container`` provides a number of transformation objects that can be applied to containers to alter the appearance
of items passed to ``for_all`` operations without modifying the items within the container itself. The currently
supported transformation objects are:

   * ``filter`` - Filters items in a container to only execute on the portion of the container satisfying a provided
     boolean function.
   * ``flatten`` - Extract the elements from tuple-like objects before passing to the user's ``for_all`` function.
   * ``map`` - Apply a generic function to the container's items before passing to the user's ``for_all`` function.

Container Class Documentation
=============================

.. toctree::
   :maxdepth: 1
   :caption: Container Classes:

   container/array
   container/bag
   container/counting_set
   container/disjoint_set
   container/map
   container/set

