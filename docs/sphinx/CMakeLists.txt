#
# Sphinx
#
find_package(Sphinx)
if (Sphinx_FOUND)
    set(SPHINX_SOURCE ${CMAKE_CURRENT_SOURCE_DIR})
    set(SPHINX_BUILD ${CMAKE_CURRENT_BINARY_DIR}/sphinx)
    set(SPHINX_INDEX_FILE ${SPHINX_BUILD}/index.html)
    set(SPHINX_LATEX_DIR ${SPHINX_BUILD}/latex)
    set(SPHINX_PDF_FILE ${SPHINX_LATEX_DIR}/ygm.pdf)

    file(
            GLOB_RECURSE SPHINX_DEPENDS_FILES
            LIST_DIRECTORIES false
            "${SPHINX_SOURCE}/*.rst" "${DOXYGEN_XML_OUTPUT}/*"
    )

    # Only regenerate Sphinx when: - Our doc files have been updated - Doxygen
    # has rerun - The Sphinx config has been updated
    add_custom_command(
            OUTPUT ${SPHINX_INDEX_FILE}
            COMMAND
            ${SPHINX_EXECUTABLE} -b html
            # Tell Breathe where to find the Doxygen output
            -Dbreathe_projects.ygm=${DOXYGEN_XML_OUTPUT} ${SPHINX_SOURCE}
            ${SPHINX_BUILD}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            DEPENDS doxygen ${SPHINX_DEPENDS_FILES}
            MAIN_DEPENDENCY ${SPHINX_SOURCE}/conf.py
            COMMENT "Generating documentation using Sphinx"
    )

    # Create a command to generate PDF document from Sphinx
    add_custom_command(
            OUTPUT ${SPHINX_PDF_FILE}
            COMMAND
            ${SPHINX_EXECUTABLE} -b latex
            -Dbreathe_projects.ygm=${DOXYGEN_XML_OUTPUT} ${SPHINX_SOURCE}
            ${SPHINX_LATEX_DIR}
            # Command to compile LaTex to get PDF
            COMMAND
            make -C ${SPHINX_LATEX_DIR}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            DEPENDS doxygen ${SPHINX_DEPENDS_FILES}
            MAIN_DEPENDENCY ${SPHINX_SOURCE}/conf.py
            COMMENT "Generating PDF documentation using Sphinx"
      )

    # Nice named target so we can run the job easily
    add_custom_target(sphinx DEPENDS ${SPHINX_INDEX_FILE})
    add_custom_target(sphinx-pdf DEPENDS ${SPHINX_PDF_FILE})

    # Add an install target to install the docs
    include(GNUInstallDirs)
    install(DIRECTORY ${SPHINX_BUILD} DESTINATION ${CMAKE_INSTALL_DOCDIR})
else ()
    message(STATUS "Sphinx not found")
endif ()
