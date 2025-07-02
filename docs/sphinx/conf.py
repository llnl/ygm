# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = 'YGM'
copyright = '2024, YGM Developers'
author = 'YGM Developers'

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = ["sphinx_rtd_theme", # Read the Docs theme
              'breathe', # Breathe
              "sphinx.ext.graphviz" # use Sphinx’s graphviz
              ]

templates_path = ['_templates']
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']



# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = 'sphinx_rtd_theme'
html_static_path = ['_static']

# Breathe Configuration
breathe_projects = {"ygm": "https://github.com/LLNL/ygm"}
breathe_default_project = "ygm"



# -- Run Doxygen -------------------------------------------------
import os
import subprocess


def run_cmake_build(build_dir):
    # -- The following commands are executed in the docs directory -- #
    try:
        subprocess.run(["mkdir", build_dir], check=True)
        # save the current working directory
        wd = os.getcwd()
        os.chdir(build_dir)
        # Use the CMakeLists.txt file in the root directory
        subprocess.run(["cmake", "../../../", "-DYGM_DOCS_ONLY=ON"], check=True)
        # Generate the doxygen xml files
        subprocess.run(["make", "doxygen"], check=True)
        # Back to the original working directory
        os.chdir(wd)

    except subprocess.CalledProcessError as e:
        print(f"Failed to run cmake build: {e}")
        raise


# Check if we're running on Read the Docs' servers
read_the_docs_build = os.environ.get('READTHEDOCS', None) == 'True'

if read_the_docs_build:
    build_dir = "build-doc"
    run_cmake_build(build_dir)
    # Breathe Configuration
    # Specify the directory where the doxygen xml files are generated,
    # which is determined by the CMakeLists.txt file in docs/
    breathe_projects = {"ygm": build_dir + "/docs/xml/"}
