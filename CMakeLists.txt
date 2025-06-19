# CMake Minimum version
cmake_minimum_required(VERSION 3.7)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

SET(XKRT xkrt)

# Project name
project(${XKRT})

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

###########
# OPTIONS #
###########

option(ENABLE_HEAVY_DEBUG "Heavy assertion tests in the khp-tree structure, setting this variable to 'ON' will considerably slowdown task insertion, but can help debugging the khp-trees" OFF)
option(STRICT       "Enable strict compilation mode" ON)
option(USE_CAIRO    "Use Cairo to generate images (memory-tree debugging purposes mostly)." OFF)
option(USE_CL       "Whether to build the OpenCL driver." OFF)
option(USE_CUDA     "Whether to build the CUDA driver." OFF)
option(USE_HIP      "Whether to build the HIP driver." OFF)
option(USE_NVML     "Use Nvidia's NVML management library." OFF)
option(USE_RSMI     "Use AMD's RSMI management library." OFF)
option(USE_STATS    "Whether to compile the built-in stat system." OFF)
option(USE_SYCL     "Whether to build the SYCL driver." OFF)
option(USE_ZE       "Whether to build the Level Zero driver." OFF)
option(USE_ZES      "Use Intel's ZES management library." OFF)
option(USE_ZE_SYCL_INTEROP "Enable the compilation of SYCL interop with Level Zero driver" OFF)
option(USE_MEMORY_REGISTER_OVERFLOW_PROTECTION "Enable the protection of memory copies over regions that are not continuously registered, by splitting into multiple copies, to avoid cuda crashes" ON)
option(USE_SHUT_UP  "Disable all logging activities" OFF)

include(BooleanUtils)
convert_bool(ENABLE_HEAVY_DEBUG)
convert_bool(STRICT)
convert_bool(USE_CAIRO)
convert_bool(USE_CL)
convert_bool(USE_CUDA)
convert_bool(USE_HIP)
convert_bool(USE_NVML)
convert_bool(USE_RSMI)
convert_bool(USE_STATS)
convert_bool(USE_SYCL)
convert_bool(USE_ZE)
convert_bool(USE_ZES)
convert_bool(USE_ZE_SYCL_INTEROP)
convert_bool(USE_MEMORY_REGISTER_OVERFLOW_PROTECTION)
convert_bool(USE_SHUT_UP)

configure_file(
    ${CMAKE_SOURCE_DIR}/include/xkrt/support.h.in
    ${CMAKE_SOURCE_DIR}/include/xkrt/support.h
)

# XKRT version and supports
add_custom_command(
    OUTPUT  always-run
    COMMAND python3 ${CMAKE_SOURCE_DIR}/utils/version.py ${CMAKE_SOURCE_DIR}/include/xkrt version
    COMMENT "Always generate version file before building the project"
)
add_custom_target(always_generate_files ALL DEPENDS always-run)

########
# XKRT #
########

SET(SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/src/conf/conf.cc
    ${CMAKE_SOURCE_DIR}/src/debug/cairo.cc
    ${CMAKE_SOURCE_DIR}/src/driver.cc
    ${CMAKE_SOURCE_DIR}/src/driver/device.cc
    ${CMAKE_SOURCE_DIR}/src/driver/driver.cc
    ${CMAKE_SOURCE_DIR}/src/driver/stream.cc
    ${CMAKE_SOURCE_DIR}/src/file/readwrite.cc
    ${CMAKE_SOURCE_DIR}/src/logger/bits-to-str.cc
    ${CMAKE_SOURCE_DIR}/src/logger/logger.cc
    ${CMAKE_SOURCE_DIR}/src/logger/metric.cc
    ${CMAKE_SOURCE_DIR}/src/memory.cc
    ${CMAKE_SOURCE_DIR}/src/memory/coherent.cc
    ${CMAKE_SOURCE_DIR}/src/memory/copy.cc
    ${CMAKE_SOURCE_DIR}/src/memory/distribute.cc
    ${CMAKE_SOURCE_DIR}/src/memory/invalidate.cc
    ${CMAKE_SOURCE_DIR}/src/memory/register.cc
    ${CMAKE_SOURCE_DIR}/src/memory/register-legacy.cc
    ${CMAKE_SOURCE_DIR}/src/memory/replicate.cc
    ${CMAKE_SOURCE_DIR}/src/power.cc
    ${CMAKE_SOURCE_DIR}/src/runtime.cc
    ${CMAKE_SOURCE_DIR}/src/sched.cc
    ${CMAKE_SOURCE_DIR}/src/stats/stats.cc
    ${CMAKE_SOURCE_DIR}/src/task.cc
    ${CMAKE_SOURCE_DIR}/src/task/task-format.cc
    ${CMAKE_SOURCE_DIR}/src/task/task.cc
    ${CMAKE_SOURCE_DIR}/src/thread/deque.cc
    ${CMAKE_SOURCE_DIR}/src/thread/thread.cc
)

# XKRT Lib
add_library(${XKRT} SHARED ${SOURCE_FILES})
add_dependencies(${XKRT} always_generate_files)
set_target_properties(${XKRT} PROPERTIES BUILD_RPATH "${CMAKE_SOURCE_DIR}/lib")
set_target_properties(${XKRT} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

# Include directory
include_directories(include/)
include_directories(src/)

# Installation
install(TARGETS ${XKRT} LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include FILES_MATCHING PATTERN "*.h*")

###############
# DEPENDENCES #
###############

# PTHREAD
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(${XKRT} PRIVATE Threads::Threads)

# HWLOC
find_package(HWLOC REQUIRED)
include_directories(${HWLOC_INCLUDE_DIRS})
target_link_libraries(${XKRT} PRIVATE ${HWLOC_LIBRARIES})

# Host driver
add_library(${XKRT}_driver_host SHARED src/driver/driver_host.cc)
add_dependencies(${XKRT}_driver_host always_generate_files)
target_link_libraries(${XKRT} PRIVATE ${XKRT}_driver_host)
install(TARGETS ${XKRT}_driver_host LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)

# CUDA
if (USE_CUDA)

    find_package(CUDA_DRIVER REQUIRED)
    include_directories("${CUDA_DRIVER_INCLUDE_DIRS}")

    find_package(CUDA_RT REQUIRED)
    include_directories("${CUDA_RT_INCLUDE_DIRS}")

    find_package(CUBLAS REQUIRED)
    include_directories("${CUBLAS_INCLUDE_DIRS}")

    add_library(${XKRT}_driver_cu SHARED src/driver/driver_cu.cc)
    add_dependencies(${XKRT}_driver_cu always_generate_files)
    target_link_libraries(${XKRT}_driver_cu PRIVATE ${HWLOC_LIBRARIES} ${CUDA_DRIVER_LIBRARIES} ${CUDA_RT_LIBRARIES} ${CUBLAS_LIBRARIES})
    target_link_libraries(${XKRT} PRIVATE ${XKRT}_driver_cu)
    install(TARGETS ${XKRT}_driver_cu LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)

endif()

# HIP
if (USE_HIP)

    find_package(HIP REQUIRED)
    include_directories("${HIP_INCLUDE_DIRS}")

    find_package(HIPBLAS REQUIRED)
    include_directories("${HIPBLAS_INCLUDE_DIRS}")

    add_library(${XKRT}_driver_hip SHARED src/driver/driver_hip.cc)
    add_dependencies(${XKRT}_driver_hip always_generate_files)
    target_link_libraries(${XKRT}_driver_hip PRIVATE ${HWLOC_LIBRARIES} ${HIP_LIBRARIES} ${HIPBLAS_LIBRARIES})
    message(STATUS "LIB IS ${HIP_LIBRARIES}")
    target_link_libraries(${XKRT} PRIVATE ${XKRT}_driver_hip)
    install(TARGETS ${XKRT}_driver_hip LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)

endif()

# Level Zero
if (USE_ZE)

    find_package(ZE REQUIRED)
    include_directories("${ZE_INCLUDE_DIRS}")

    add_library(${XKRT}_driver_ze SHARED src/driver/driver_ze.cc)
    add_dependencies(${XKRT}_driver_ze always_generate_files)
    target_link_libraries(${XKRT} PRIVATE ${XKRT}_driver_ze)
    install(TARGETS ${XKRT}_driver_ze LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)

endif()

# SYCL / ZE_SYCL_INTEROP
if (USE_SYCL OR USE_ZE_SYCL_INTEROP)
    find_package(SYCL REQUIRED)
    include_directories(${SYCL_INCLUDE_DIRS})
endif()

if (USE_SYCL)
    add_library(${XKRT}_driver_sycl SHARED src/driver/driver_sycl.cc)
    target_link_libraries( ${XKRT}_driver_sycl PRIVATE ${SYCL_LIBRARIES} -fsycl)
    target_compile_options(${XKRT}_driver_sycl PRIVATE -fsycl)
    add_dependencies(      ${XKRT}_driver_sycl always_generate_files)
    target_link_libraries(${XKRT} PRIVATE ${XKRT}_driver_sycl)
    install(TARGETS ${XKRT}_driver_sycl LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
endif()

if (USE_ZE_SYCL_INTEROP)
    target_link_libraries( ${XKRT}_driver_ze PRIVATE ${SYCL_LIBRARIES} -fsycl)
    target_compile_options(${XKRT}_driver_ze PRIVATE -fsycl)
endif()

# OpenCL
if (USE_CL)
    find_package(CL REQUIRED)
    include_directories("${CL_INCLUDE_DIRS}")

    add_library(${XKRT}_driver_cl SHARED src/driver/driver_cl.cc)
    add_dependencies(${XKRT}_driver_cl always_generate_files)
    target_link_libraries(${XKRT} PRIVATE ${XKRT}_driver_cl ${CL_LIBRARIES})
    install(TARGETS ${XKRT}_driver_cl LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)

endif()

# NVML
if (USE_NVML)
    find_package(NVML REQUIRED)
    include_directories("${NVML_INCLUDE_DIRS}")
    target_link_libraries(${XKRT} PRIVATE ${NVML_LIBRARIES})
endif()

# ZES (Intel energy)
if (USE_ZES)
    # nothing to do, but must USE_ZE to have any effects
endif()

# RSMI
if (USE_RSMI)
    find_package(RSMI REQUIRED)
    include_directories("${RSMI_INCLUDE_DIRS}")
    target_link_libraries(${XKRT} PRIVATE ${RSMI_LIBRARIES})
endif()

# Cairo
if (USE_CAIRO)
    find_package(CAIRO REQUIRED)
    include_directories("${CAIRO_INCLUDE_DIRS}")
    target_link_libraries(${XKRT} PRIVATE ${CAIRO_LIBRARIES})
endif()

if (STRICT)
    SET(STRICT_OPTIONS -Wall -Wextra -Wfatal-errors -Werror=conversion -ftrapv -Wno-sign-conversion)
    target_compile_options(${XKRT} PRIVATE ${STRICT_OPTIONS})
    if (${CUDA_DRIVER_FOUND})
        target_compile_options(${XKRT}_driver_cu PRIVATE ${STRICT_OPTIONS})
    endif()
endif()

################
# Sub projects #
################
add_subdirectory(tests/)
enable_testing()
