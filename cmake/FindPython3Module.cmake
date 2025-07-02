# Find a Python3 module using CMake's FindPython3 module.
# Input: module name to find
# Python3_ROOT_DIR can be used as a hint to find Python3
#
# Output: PYTHON3_MODULE_PATH is set to the path of the module if found
function(find_python3_module module_name)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)

    execute_process(
            COMMAND ${Python3_EXECUTABLE} -c "import importlib; import sys; module_name = '${module_name}'; spec = importlib.util.find_spec(module_name); print(spec.origin if spec else ''); sys.exit(0 if spec else 1)"
            OUTPUT_VARIABLE MODULE_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if (Python3_FOUND AND MODULE_PATH)
        set(PYTHON3_MODULE_PATH ${MODULE_PATH} PARENT_SCOPE)
    endif ()
endfunction()