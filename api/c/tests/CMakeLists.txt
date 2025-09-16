# CMake Minimum version
cmake_minimum_required(VERSION 3.10)
include(CTest)

# Project name
project(tests)

# list of tests
set(TEST_SOURCES
    task-spawn.c
)

# Loop over each test source file
foreach(TEST_FILE ${TEST_SOURCES})
    # Get the file name without the extension (e.g., "test" from "test.c")
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)

    # Create test
    add_executable(${TEST_NAME} ${TEST_FILE})
    target_link_libraries(${TEST_NAME} PRIVATE ${CXKRT} XKRT::xkrt)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})

endforeach()
