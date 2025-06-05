# Try to find HIPBLAS headers and libraries.
#
# Usage of this module as follows:
#
#     find_package(HIPBLASLocal)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  HIPBLAS_PREFIX          Set this variable to the root installation of
#                      libamdhip64.so if the module has problems finding the
#                      proper installation path.
#
# Variables defined by this module:
#
#  HIPBLAS_FOUND              System has HIPBLAS libraries and headers
#  HIPBLAS_LIBRARIES          The HIPBLAS library
#  HIPBLAS_INCLUDE_DIRS       The location of HIPBLAS headers

find_path(HIPBLAS_PREFIX
    NAMES include/hipblas/hipblas.h
    HINTS $ENV{HIPBLAS_PATH} $ENV{HIP_PATH}
)

find_library(HIPBLAS_LIBRARIES
    # Pick the static library first for easier run-time linking.
    NAMESi libhipblas.so
    HINTS ${HIPBLAS_PREFIX}/lib ${HILTIDEPS}/lib
)

find_path(HIPBLAS_INCLUDE_DIRS
    NAMES hipblas/hipblas.h
    HINTS ${HIPBLAS_PREFIX}/include ${HILTIDEPS}/include
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(HIPBLAS DEFAULT_MSG
    HIPBLAS_LIBRARIES
    HIPBLAS_INCLUDE_DIRS
)

mark_as_advanced(
    HIPBLAS_PREFIX_DIRS
    HIPBLAS_LIBRARIES
    HIPBLAS_INCLUDE_DIRS
)

