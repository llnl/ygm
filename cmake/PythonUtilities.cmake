# Create and activate a Python3 virtual environment
#
# Output: PYTHON_VENV_ROOT is set to the path of the virtual environment
# if created successfully
function(setup_python_venv)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if (NOT Python3_Interpreter_FOUND)
        message(WARNING "Python3 interpreter not found")
        return()
    endif()

    set(PYTHON_VENV_ROOT "${CMAKE_BINARY_DIR}/${PROJECT_NAME}-venv")
    execute_process(
            COMMAND ${Python3_EXECUTABLE} -m venv ${PYTHON_VENV_ROOT}
            RESULT_VARIABLE result
            OUTPUT_QUIET
    )
    if (result EQUAL "0")
        message(STATUS "Created Python virtual environment in ${PYTHON_VENV_ROOT}")
        set(PYTHON_VENV_ROOT ${PYTHON_VENV_ROOT} PARENT_SCOPE)
    endif()
endfunction()

# Activate a Python3 virtual environment
# Input: A path to the virtual environment
# Output: PYTHON_VENV_ACTIVATED is set to TRUE if activated successfully
function(activate_python_venv venv_path)
    set (ENV{VIRTUAL_ENV} ${venv_path})
    set(PYTHON_VENV_ACTIVATED TRUE PARENT_SCOPE)
endfunction()

# Deactivate a Python3 virtual environment
function(deactivate_python_venv)
    unset(ENV{VIRTUAL_ENV})
    set(PYTHON_VENV_ACTIVATED FALSE PARENT_SCOPE)
endfunction()

# Upgrade pip in the Python3 interpreter
# Output: PIP_UPGRADE_SUCCEEDED is set to TRUE if pip was upgraded successfully
function(upgrade_pip)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if (NOT Python3_Interpreter_FOUND)
        message(WARNING "Python3 interpreter not found")
        return()
    endif()

    execute_process(
            COMMAND ${Python3_EXECUTABLE} -m pip install --upgrade --no-user pip
            RESULT_VARIABLE result
            OUTPUT_QUIET
    )
    if(result EQUAL "0")
        set(PIP_UPGRADE_SUCCEEDED TRUE PARENT_SCOPE)
    endif()
endfunction()

# Install a Python3 package using pip
#
# Input: A path to pip_executable and a package name
# Output: PIP_INSTALL_SUCCEEDED is set to TRUE
# if the package was installed successfully
function(pip_install_python_package package_name)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if (NOT Python3_Interpreter_FOUND)
        message(WARNING "Python3 interpreter not found")
        return()
    endif()

    execute_process(
            COMMAND ${Python3_EXECUTABLE} -m pip install --no-user ${package_name}
            RESULT_VARIABLE result
            OUTPUT_QUIET
    )
    if(result EQUAL "0")
        message(STATUS "Installed ${package_name}")
        set(PIP_INSTALL_SUCCEEDED TRUE PARENT_SCOPE)
    endif()
endfunction()

# Find a Python3 module using CMake's FindPython3 module.
# Input: module name to find
# Python3_ROOT_DIR can be used as a hint to find Python3
#
# Output: PYTHON3_MODULE_PATH is set to the path of the module if found
function(find_python3_module module_name)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if (NOT Python3_Interpreter_FOUND)
        message(WARNING "Python3 interpreter not found")
        return()
    endif()

    execute_process(
            COMMAND ${Python3_EXECUTABLE} -c "import importlib.util; import sys; module_name = '${module_name}'; spec = importlib.util.find_spec(module_name); print(spec.origin if spec else ''); sys.exit(0 if spec else 1)"
            OUTPUT_VARIABLE MODULE_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE result
    )

    if (result EQUAL "0")
        set(PYTHON3_MODULE_PATH ${MODULE_PATH} PARENT_SCOPE)
        message(STATUS "Found Python module ${module_name} at ${MODULE_PATH}")
    endif()
endfunction()
