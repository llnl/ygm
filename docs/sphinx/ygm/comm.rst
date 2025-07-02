.. _ygm-comm:

:code:`ygm::comm` class reference.
==================================

Communicator Overview
---------------------

The communicator :code:`ygm::comm` is the central object in YGM.
The communicator controls an interface to an MPI communicator, and its
functionality can be modified by additional optional parameters.

Communicator Features:
   * **Message Buffering** - Increases application throughput at the expense of increased message latency.
   * **Message Routing** - Extends benefits of message buffering to extremely large HPC allocations.
   * **Fire-and-Forget RPC Semantics** - A sender provides the function and function arguments for execution on a specified
     destination rank through an `async` call. This function will complete on the destination rank at an unspecified time
     in the future, but YGM does not explicitly make the sender aware of this completion.

Communicator Hello World
------------------------

Here we will walk through a basic "hello world" YGM program. The `examples directory`_ in the `YGM tutorial`_ contains several other
examples, including many using YGM's storage containers.

To begin, headers for a YGM communicator are needed:
   
.. code-block:: C++

   #include <ygm/comm.hpp>

At the beginning of the program, a YGM communicator must be constructed. It will be given ``argc`` and ``argv`` like
``MPI_Init``.

.. code-block:: C++

   ygm::comm world(&argc, &argv);

Next, we need a lambda to send through YGM. We'll do a simple hello\_world type of lambda.

.. code-block:: C++

   auto hello_world_lambda = [](const std::string &name) {
	   std::cout << "Hello " << name << std::endl;
   };

Finally, we use this lambda inside of our `async` calls. In this case, we will have rank 0 send a message to rank 1,
telling it to greet the world

.. code-block:: C++

   if (world.rank0()) {
	   world.async(1, hello_world_lambda, std::string("world"));
   }

A full, compilable version of this example is found `here`_. 

.. _examples directory: https://github.com/LLNL/ygm-tutorial/tree/main/examples
.. _YGM tutorial: https://github.com/LLNL/ygm-tutorial
.. _here: https://github.com/LLNL/ygm-tutorial/blob/main/examples/howdy_world.cpp

.. toctree::
   :maxdepth: 1
   :caption: Communicator API

   comm/comm
