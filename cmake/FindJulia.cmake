# FindJulia.cmake
# Locate Julia installation, headers, and library
# Sets:
#   Julia_FOUND
#   Julia_EXECUTABLE
#   Julia_INCLUDE_DIRS
#   Julia_LIBRARIES
#   Julia_VERSION

cmake_minimum_required(VERSION 3.12)

find_program(JULIA_EXECUTABLE julia
    HINTS ENV PATH
)

if(NOT JULIA_EXECUTABLE)
    message(FATAL_ERROR "Could not find Julia executable")
endif()

# Get Julia prefix (install path)
execute_process(
    COMMAND "${JULIA_EXECUTABLE}" -e "print(Sys.BINDIR)"
    OUTPUT_VARIABLE JULIA_BINDIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Assume standard layout: ../include, ../lib
get_filename_component(JULIA_PREFIX "${JULIA_BINDIR}" DIRECTORY)

set(Julia_INCLUDE_DIRS "${JULIA_PREFIX}/include/julia")

# Detect platform-specific library
if(WIN32)
    set(Julia_LIBRARIES "${JULIA_PREFIX}/bin/libjulia.dll.lib")
elseif(APPLE)
    set(Julia_LIBRARIES "${JULIA_PREFIX}/lib/libjulia.dylib")
else()
    set(Julia_LIBRARIES "${JULIA_PREFIX}/lib/libjulia.so")
endif()

# Get Julia version
execute_process(
    COMMAND "${JULIA_EXECUTABLE}" -e "print(VERSION)"
    OUTPUT_VARIABLE Julia_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Success
set(Julia_FOUND TRUE CACHE BOOL "Found Julia")
set(Julia_EXECUTABLE "${JULIA_EXECUTABLE}" CACHE FILEPATH "Julia executable")

