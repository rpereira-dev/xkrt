# Try to find SYCL headers and libraries.
#
# Usage of this module as follows:
#
#     find_package(SYCL)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  SYCL_PREFIX         Set this variable to the root installation of
#                      libpapi if the module has problems finding the
#                      proper installation path.
#
# Variables defined by this module:
#
#  SYCL_FOUND              System has SYCL libraries and headers
#  SYCL_LIBRARIES          The SYCL library
#  SYCL_INCLUDE_DIRS       The location of SYCL headers

find_library(SYCL_LIBRARIES NAMES libsycl.so)
find_path(SYCL_INCLUDE_DIRS NAMES sycl/sycl.hpp)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SYCL DEFAULT_MSG
    SYCL_LIBRARIES
    SYCL_INCLUDE_DIRS
)

mark_as_advanced(
    SYCL_LIBRARIES
    SYCL_INCLUDE_DIRS
)
