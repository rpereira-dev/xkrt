# Try to find XKRT headers and libraries.
#
# Usage of this module as follows:
#
#     find_package(XKRT)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  XKRT_PREFIX         Set this variable to the root installation of
#                      libpapi if the module has problems finding the
#                      proper installation path.
#
# Variables defined by this module:
#
#  XKRT_FOUND              System has XKRT libraries and headers
#  XKRT_LIBRARIES          The XKRT library
#  XKRT_INCLUDE_DIRS       The location of XKRT headers

find_library(XKRT_LIBRARIES NAMES libxkrt.so)
find_path(XKRT_INCLUDE_DIRS NAMES xkrt/xkrt.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(XKRT DEFAULT_MSG
    XKRT_LIBRARIES
    XKRT_INCLUDE_DIRS
)

mark_as_advanced(
    XKRT_LIBRARIES
    XKRT_INCLUDE_DIRS
)
