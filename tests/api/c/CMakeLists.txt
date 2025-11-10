# CMake Minimum version
cmake_minimum_required(VERSION 3.10)
include(CTest)

# list of tests
set(TEST_SOURCES

    # C API tests
    task-with-access.c

)

# Loop over each test source file
foreach(TEST_FILE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    add_executable(${TEST_NAME}_C ${TEST_FILE})
    target_link_libraries(${TEST_NAME}_C PRIVATE ${XKRT})
    add_test(NAME ${TEST_NAME}_C COMMAND ${TEST_NAME}_C)
endforeach()
