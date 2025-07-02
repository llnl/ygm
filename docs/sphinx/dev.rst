Developing YGM
===========

This page contains information for YGM developers.

Build Read the Docs (RTD)
-------

Here is how to build RTD document using Sphinx on your machine.

.. code-block:: shell
  :caption: How to build RTD docs locally

  # Install required software
  brew install doxygen graphviz sphinx-doc
  pip install breathe sphinx_rtd_theme

  # Set PATH and PYTHONPATH, if needed
  # For example:
  # export PATH="/opt/homebrew/opt/sphinx-doc/bin:${PATH}"
  # export PYTHONPATH="/path/to/python/site-packages:${PYTHONPATH}"

  git clone https://github.com/LLNL/ygm.git
  cd ygm
  mkdir build && cd build

  # Run CMake
  cmake ../ -DYGM_RTD_ONLY=ON

  # Generate Read the Docs documents using Sphinx
  # This command runs Doxygen to generate XML files
  # before Sphinx automatically
  make sphinx
  # Open the following file using a web browser
  open docs/rtd/sphinx/index.html

  # For running doxygen only
  make doxygen
  # open the following file using a web browser
  open docs/html/index.html

Rerunning Build Command
^^^^^

Depending on what files are modified, one may need to rerun the CMake command and/or :code:`make sphinx`.
For instance:

* Require running the CMake command and :code:`make sphinx`:

  * Adding new RTD-related files, including configuration and .rst files
  * Modifying CMake files

* Require running only :code:`make sphinx`

  * **Existing** files (except CMake) are modified