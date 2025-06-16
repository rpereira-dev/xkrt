# CMake Minimum version
cmake_minimum_required(VERSION 3.10)
include(CTest)

# Project name
project(tests)

# list of tests
set(TEST_SOURCES
    deinit.cc
    fib-task-capture.cc
    fib-task-format.cc
    init.cc
    memory-register-async-legacy-memcpy-split.cc
    memory-register-async-legacy.cc
    memory-register-async.cc
    memory-touch-async.cc
    memory-touch-register-unregister-async.cc
    memory-unregister-async.cc
    sync.cc
    task-dependency-interval-matrix.cc
    task-dependency-interval.cc
    task-dependency-point.cc
    task-dependency.cc
    task-format-host.cc
    task-format.cc
    task-gpu-empty.cc
    team-cpus-master-member.cc
    team-cpus-parallel-for.cc
    team-cpus.cc
    team-device.cc
)

# pthread deps
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Loop over each test source file
foreach(TEST_FILE ${TEST_SOURCES})
    # Get the file name without the extension (e.g., "test" from "test.c")
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)

    # Create test
    add_executable(${TEST_NAME} ${TEST_FILE})
		target_link_libraries(${TEST_NAME} PRIVATE ${XKRT} ${HWLOC_LIBRARIES} ${CUDA_DRIVER_LIBRARIES})
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})

    target_link_libraries(${TEST_NAME} PRIVATE Threads::Threads)

endforeach()
