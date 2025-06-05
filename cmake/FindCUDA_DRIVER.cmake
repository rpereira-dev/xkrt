# Try to find CUDA_DRIVER headers and libraries.
#
# Usage of this module as follows:
#
#     find_package(CUDA_DRIVER)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  CUDA_DRIVER_PREFIX         Set this variable to the root installation of
#                      cuda if the module has problems finding the
#                      proper installation path.
#
# Variables defined by this module:
#
#  CUDA_DRIVER_FOUND              System has CUDA_DRIVER libraries and headers
#  CUDA_DRIVER_LIBRARIES          The CUDA_DRIVER library for Driver API
#  CUDA_DRIVER_INCLUDE_DIRS       The location of CUDA_DRIVER headers

find_path(CUDA_DRIVER_PREFIX
    NAMES include/cuda.h
)

find_library(CUDA_DRIVER_LIBRARIES
    # Pick the static library first for easier run-time linking.
    NAMES cuda
    HINTS ${CUDA_DRIVER_PREFIX}/lib64 ${CUDA_DRIVER_PREFIX}/lib64/stubs ${CUDA_DRIVER_PREFIX}/lib ${HILTIDEPS}/lib
)

find_path(CUDA_DRIVER_INCLUDE_DIRS
    NAMES cuda.h
    HINTS ${CUDA_DRIVER_PREFIX}/include ${HILTIDEPS}/include
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CUDA_DRIVER DEFAULT_MSG
    CUDA_DRIVER_LIBRARIES
    CUDA_DRIVER_INCLUDE_DIRS
)

mark_as_advanced(
    CUDA_DRIVER_PREFIX_DIRS
    CUDA_DRIVER_LIBRARIES
    CUDA_DRIVER_INCLUDE_DIRS
)
