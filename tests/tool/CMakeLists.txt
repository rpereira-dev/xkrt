# XKRT tooling interface (XKRT-T) tests.
#
# Builds:
#   - a sample tool, as a loadable module (shared object) exporting
#     `xkrt_tool_start`, that a runtime loads through XKRT_TOOL_PATH;
#   - a driver program that exercises the instrumented runtime paths.
# The driver is run with XKRT_TOOL_PATH set to the sample tool; the test passes
# when the tool prints the "XKRT-T:OK" marker from its finalize callback.

# --- sample tool (.so) -------------------------------------------------------
# An XKRT-T tool is an XKRT-aware C++ component: it uses the XKRT types and calls
# runtime methods (tool_set_callback, tool_unique_id), so it links libxkrt.so
# (a single instance is shared with the process that dlopen's the tool).
add_library(xkrt_tool_sample MODULE tool.cc)
target_link_libraries(xkrt_tool_sample PRIVATE ${XKRT})
target_include_directories(xkrt_tool_sample PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include)
set_target_properties(xkrt_tool_sample PROPERTIES CXX_STANDARD 20)

# --- driver ------------------------------------------------------------------
add_executable(tool-basic tool-basic.cc)
target_link_libraries(tool-basic PRIVATE ${XKRT})
target_include_directories(tool-basic PRIVATE ${XKRT_TESTS_ROOT})
target_compile_options(tool-basic PRIVATE -UNDEBUG -g)

add_test(NAME tool-basic COMMAND tool-basic)
set_tests_properties(tool-basic PROPERTIES
    ENVIRONMENT "XKRT_TOOL_PATH=$<TARGET_FILE:xkrt_tool_sample>"
    PASS_REGULAR_EXPRESSION "XKRT-T:OK"
    FAIL_REGULAR_EXPRESSION "XKRT-T:MISSING-EVENTS"
    LABELS "host;tool"
    TIMEOUT 30)
