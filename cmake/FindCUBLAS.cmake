# Try to find CUBLAS headers and libraries.
#
# Usage of this module as follows:
#
#     find_package(CUBLAS)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  CUBLAS_PREFIX      Set this variable to the root installation of
#                     cuda if the module has problems finding the
#                     proper installation path.
#
# Variables defined by this module:
#
#  CUBLAS_FOUND              System has CUBLAS libraries and headers
#  CUBLAS_LIBRARIES          The CUBLAS library for Runtime API
#  CUBLAS_INCLUDE_DIRS       The location of CUBLAS headers

find_path(CUBLAS_PREFIX NAMES include/cuda.h)
find_library(CUBLAS_LIBRARIES NAMES cublas HINTS ${CUBLAS_PREFIX}/lib64 ${CUBLAS_PREFIX}/lib ${HILTIDEPS}/lib HINTS /usr/local/cuda/lib64)
find_path(CUBLAS_INCLUDE_DIRS NAMES cublas_v2.h HINTS ${CUBLAS_PREFIX}/include ${HILTIDEPS}/include HINTS /usr/local/cuda/include)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CUBLAS DEFAULT_MSG
    CUBLAS_LIBRARIES
    CUBLAS_INCLUDE_DIRS
)

mark_as_advanced(
    CUBLAS_LIBRARIES
    CUBLAS_INCLUDE_DIRS
)
