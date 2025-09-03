# CMake Minimum version
cmake_minimum_required(VERSION 3.7)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

SET(CXKRT cxkrt)

# Project name
project(${CXKRT})

###############
# C++ version #
###############
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # Optional: disables compiler-specific extensions

#########
# RPATH #
#########
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

########
# CXKRT #
########

SET(SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/src/xkrt.cc
)

# CXKRT Lib
add_library(${CXKRT} SHARED ${SOURCE_FILES})
set_target_properties(${CXKRT} PROPERTIES BUILD_RPATH "${CMAKE_SOURCE_DIR}/lib")
set_target_properties(${CXKRT} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

# Include directory
include_directories(${CMAKE_SOURCE_DIR}/include/)
include_directories(${CMAKE_SOURCE_DIR}/src/)

# targets to install (lib)
set(CXKRT_TARGETS ${CXKRT})

###############
# DEPENDENCES #
###############

# XK Runtime
find_package(XKRT REQUIRED)
target_link_libraries(${CXKRT} PRIVATE XKRT::xkrt)

################
# INSTALLATION #
################

# Installation - lib and include files
install(TARGETS ${CXKRT_TARGETS} LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include FILES_MATCHING PATTERN "*.h")

# Installation - cmake files
target_include_directories(${CXKRT} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
message(STATUS "Targets are ${CXKRT_TARGETS}")
install(TARGETS ${CXKRT_TARGETS}
        EXPORT CXKRTTargets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)
install(EXPORT CXKRTTargets
        FILE CXKRTTargets.cmake
        NAMESPACE CXKRT::
        DESTINATION lib/cmake/CXKRT)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    cmake/CXKRTConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/CXKRTConfig.cmake"
    INSTALL_DESTINATION lib/cmake/CXKRT
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/CXKRTConfig.cmake"
    DESTINATION lib/cmake/CXKRT
)

################
# Sub projects #
################
add_subdirectory(tests/)
