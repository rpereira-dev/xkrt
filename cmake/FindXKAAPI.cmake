# Try to find XKAAPI headers and libraries.
#
# Usage of this module as follows:
#
#     find_package(XKAAPI)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  XKAAPI_PREFIX         Set this variable to the root installation of
#                      libpapi if the module has problems finding the
#                      proper installation path.
#
# Variables defined by this module:
#
#  XKAAPI_FOUND              System has XKAAPI libraries and headers
#  XKAAPI_LIBRARIES          The XKAAPI library
#  XKAAPI_INCLUDE_DIRS       The location of XKAAPI headers

find_library(XKAAPI_LIBRARIES NAMES libxkrt.so)
find_path(XKAAPI_INCLUDE_DIRS NAMES xkrt/xkrt.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(XKAAPI DEFAULT_MSG
    XKAAPI_LIBRARIES
    XKAAPI_INCLUDE_DIRS
)

mark_as_advanced(
    XKAAPI_LIBRARIES
    XKAAPI_INCLUDE_DIRS
)
