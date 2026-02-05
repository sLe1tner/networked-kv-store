cmake_minimum_required(VERSION 3.20)

project(
    kv_store
    VERSION 0.1.0
    DESCRIPTION "Multithreaded TCP key-value store"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable warnings
if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(Threads REQUIRED)

enable_testing()
add_subdirectory(src)

# Add unit test subdirectory
add_subdirectory(tests/unit)

# Add Python Integration Tests
find_package(Python3 COMPONENTS Interpreter)

if(Python3_Interpreter_FOUND)
    # Check if pytest is actually installed in the Python environment
    execute_process(
        COMMAND ${Python3_EXECUTABLE} -m pytest --version
        RESULT_VARIABLE PYTEST_NOT_FOUND
        OUTPUT_QUIET
        ERROR_QUIET
    )

    if(PYTEST_NOT_FOUND EQUAL 0)
        # Get the path to server executable target
        set(SERVER_EXE_PATH $<TARGET_FILE:kv_server>)
        # This creates a test named "Integration" that runs pytest
        add_test(NAME Integration
            COMMAND ${CMAKE_COMMAND} -E env "KV_SERVER_BIN=${SERVER_EXE_PATH}"
                    ${Python3_EXECUTABLE} -m pytest
                    # Use quotes to ensure it handles spaces correctly
                    "${CMAKE_SOURCE_DIR}/tests/integration"
                    ${EXTRA_ARGS}
        )
    else()
        message(WARNING "Pytest not found. Integration tests will be skipped. "
                        "Install it with: pip install pytest")

    endif()
endif()
