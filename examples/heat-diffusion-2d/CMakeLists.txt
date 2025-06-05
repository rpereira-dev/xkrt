# CMake Minimum version
cmake_minimum_required(VERSION 3.7)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

# Project name
SET(HEAT_NAME heat)
project(${HEAT_NAME})

SET(SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/src/main.cc
)

SET(HEAT_CUDA_KERNELS_SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/src/kernels/diffusion.cu
)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

########
# HEAT #
########
add_executable(${HEAT_NAME} ${SOURCE_FILES})

# Include directory
include_directories(include/)
include_directories(src/)

################
# DEPENDENCIES #
################

# XK Runtime
find_package(XKRT REQUIRED)
include_directories("${XKRT_INCLUDE_DIRS}")
target_link_libraries(${HEAT_NAME} PRIVATE ${XKRT_LIBRARIES})

# Cuda kernels
option(USE_CUDA "Enable the compilation of cuda kernels" ON)
if (USE_CUDA)

    find_package(CUDA_DRIVER REQUIRED)
    find_package(CUDA_RT REQUIRED)
    include_directories("${CUDA_DRIVER_INCLUDE_DIRS}")
    include_directories("${CUDA_RT_INCLUDE_DIRS}")
    target_link_libraries(${HEAT_NAME} PRIVATE ${CUDA_DRIVER_LIBRARIES} ${CUDA_RT_LIBRARIES})

    enable_language(CUDA)
    set_source_files_properties(${HEAT_CUDA_KERNELS_SOURCE_FILES} PROPERTIES LANGUAGE CUDA)
    add_library(${HEAT_NAME}_kernels_cuda SHARED ${HEAT_CUDA_KERNELS_SOURCE_FILES})
    set_target_properties(${HEAT_NAME}_kernels_cuda PROPERTIES CUDA_ARCHITECTURES "50;72")
    install(TARGETS ${HEAT_NAME}_kernels_cuda LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
    target_link_libraries(${HEAT_NAME} PRIVATE ${HEAT_NAME}_kernels_cuda)
endif()

# Level Zero
option(USE_ZE "Enable the compilation of ze kernels" ON)
if (USE_ZE)
    find_package(ZE REQUIRED)
    include_directories("${ZE_INCLUDE_DIRS}")
    target_link_libraries(${HEAT_NAME} PRIVATE ${ZE_LIBRARIES})
endif()

# SYCL
option(USE_SYCL "Enable the compilation of SYCL and set its interop with Level Zero driver" ON)
if (USE_SYCL)
    find_package(SYCL REQUIRED)
    include_directories(${SYCL_INCLUDE_DIRS})
    target_compile_options(${HEAT_NAME} PRIVATE -fsycl)
    target_link_libraries(${HEAT_NAME} PRIVATE ${SYCL_LIBRARIES} -fsycl)
endif()

