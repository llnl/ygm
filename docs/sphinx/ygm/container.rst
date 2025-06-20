.. _ygm-container:

:code:`ygm::container` module reference.
========================================

:code:`ygm::container` is a collection of distributed containers designed specifically
to perform well within YGM's asynchronous runtime.
Inspired by C++'s Standard Template Library (STL), the containers provide
improved programmability by allowing developers to consider an algorithm as the
operations that need to be performed on the data stored in a container while
abstracting the locality and access details of said data.
While insiration is taken from STL, the top priority is to provide expressive
and performant tools within the YGM framework.

Implemented Storage Containers
------------------------------

The currently implemented containers include a mix of distributed versions of familiar containers and
distributed-specific containers:

   * ``ygm::container::bag`` - An unordered collection of objects partitioned across processes. Ideally suited for
     iteration over all items with no capability for identifying or searching for an individual item within the bag.
   * ``ygm::container::set`` - Analogous to ``std::set``. An unordered collection of unique objects with the ability to iterate
     and search for individual items. Insertion and iteration are slower than a ``ygm::container::bag``.
   * ``ygm::container::multiset`` - Analogous to ``std::multiset``. A set where multiple instances of the same object
     may appear.
   * ``ygm::container::map`` - Analogous to ``std::map``. A collection of keys with assigned values. Keys and values can
     be inserted and looked up individually or iterated over collectively.
   * ``ygm::container::multimap`` - Analogous to ``std::multimap``. A map where keys may appear with multiple values.
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
----------------------------

Most interaction with containers occurs in one of two classes of operations:
:code:`for_all` and `async_`.

:code:`for_all` Operations
^^^^^^^^^^^^^^^^^^^^^^^^^^

:code:`for_all`-class operations are barrier-inducing collectives that direct
ranks to iteratively apply a user-provided function to all locally-held data.
Functions passed to the :code:`for_all` interface do not support additional
variadic parameters.
However, these functions are stored and executed locally on each rank, and so
can capture objects in rank-local scope.

:code:`async_` Operations
^^^^^^^^^^^^^^^^^^^^^^^^^

Operations prefixed with ``async_`` perform operations on containers that can be spawned from any process and
execute on the correct process using YGM's asynchronous runtime. The most common `async` operations are:

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

.. toctree::
   :maxdepth: 2
   :caption: Container Classes:

   container/array
   container/bag
   container/counting_set
   container/disjoint_set
   container/map
   container/multimap
   container/multiset
   container/set

YGM Container Example
---------------------

.. literalinclude:: ../../../examples/container/map_visit.cpp
   :language: C++

Container Transformation Objects
--------------------------------

``ygm::container`` provides a number of transformation objects that can be applied to containers to alter the appearance
of items passed to ``for_all`` operations without modifying the items within the container itself. The currently
supported transformation objects are:

   * ``filter`` - Filters items in a container to only execute on the portion of the container satisfying a provided
     boolean function.
   * ``flatten`` - Extract the elements from tuple-like objects before passing to the user's ``for_all`` function.
   * ``map`` - Apply a generic function to the container's items before passing to the user's ``for_all`` function.
