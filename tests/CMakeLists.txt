# CMake Minimum version
cmake_minimum_required(VERSION 3.10)
include(CTest)

# Project name
project(tests)

# list of tests
set(TEST_SOURCES
    deinit.cc
    fib-task-format.cc
    fib-task-capture.cc
    init.cc
    sync.cc
    task-dependency.cc
    task-dependency-point.cc
    task-format-host.cc
    task-format.cc
    task-gpu-empty.cc
    team-cpus.cc
    team-cpus-parallel-for.cc
    team-device.cc
    touch-async.cc
)

# Loop over each test source file
foreach(TEST_FILE ${TEST_SOURCES})
    # Get the file name without the extension (e.g., "test" from "test.c")
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)

    # Create test
    add_executable(${TEST_NAME} ${TEST_FILE})
		target_link_libraries(${TEST_NAME} PRIVATE ${XKRT} ${HWLOC_LIBRARIES} ${CUDA_DRIVER_LIBRARIES})
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})

endforeach()
