# Try to find CUDNN headers and libraries.
#
# Usage of this module as follows:
#
#     find_package(CUDNN)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  CUDNN_PREFIX      Set this variable to the root installation of
#                     cuda if the module has problems finding the
#                     proper installation path.
#
# Variables defined by this module:
#
#  CUDNN_FOUND              System has CUDNN libraries and headers
#  CUDNN_LIBRARIES          The CUDNN library for Runtime API
#  CUDNN_INCLUDE_DIRS       The location of CUDNN headers

find_path(CUDNN_PREFIX NAMES include/cuda.h)
find_library(CUDNN_LIBRARIES NAMES cublas HINTS ${CUDNN_PREFIX}/lib64 ${CUDNN_PREFIX}/lib ${HILTIDEPS}/lib HINTS /usr/local/cuda/lib64)
find_path(CUDNN_INCLUDE_DIRS NAMES cublas_v2.h HINTS ${CUDNN_PREFIX}/include ${HILTIDEPS}/include HINTS /usr/local/cuda/include)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CUDNN DEFAULT_MSG
    CUDNN_LIBRARIES
    CUDNN_INCLUDE_DIRS
)

mark_as_advanced(
    CUDNN_LIBRARIES
    CUDNN_INCLUDE_DIRS
)
