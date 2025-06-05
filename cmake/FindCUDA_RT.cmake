# Try to find CUDA_RT headers and libraries.
#
# Usage of this module as follows:
#
#     find_package(CUDA_RT)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  CUDA_RT_PREFIX      Set this variable to the root installation of
#                      cuda if the module has problems finding the
#                      proper installation path.
#
# Variables defined by this module:
#
#  CUDA_RT_FOUND              System has CUDA_RT libraries and headers
#  CUDA_RT_LIBRARIES          The CUDA_RT library for Runtime API
#  CUDA_RT_INCLUDE_DIRS       The location of CUDA_RT headers

find_path(CUDA_RT_PREFIX NAMES include/cuda.h)
find_library(CUDA_RT_LIBRARIES
    NAMES cudart
    HINTS ${CUDA_RT_PREFIX}/lib64 ${CUDA_RT_PREFIX}/lib ${HILTIDEPS}/lib
    HINTS /usr/local/cuda/lib64
)

find_path(CUDA_RT_INCLUDE_DIRS
    NAMES cuda_runtime.h
    HINTS ${CUDA_RT_PREFIX}/include ${HILTIDEPS}/include
    HINTS /usr/local/cuda/include
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CUDA_RT DEFAULT_MSG
    CUDA_RT_LIBRARIES
    CUDA_RT_INCLUDE_DIRS
)

mark_as_advanced(
    CUDA_RT_LIBRARIES
    CUDA_RT_INCLUDE_DIRS
)
