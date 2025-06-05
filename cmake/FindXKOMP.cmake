# Try to find XKOMP headers and libraries.
#
# Usage of this module as follows:
#
#     find_package(XKOMP)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  XKOMP_PREFIX         Set this variable to the root installation of
#                      libpapi if the module has problems finding the
#                      proper installation path.
#
# Variables defined by this module:
#
#  XKOMP_FOUND              System has XKOMP libraries and headers
#  XKOMP_LIBRARIES          The XKOMP library
#  XKOMP_INCLUDE_DIRS       The location of XKOMP headers

find_library(XKOMP_LIBRARIES NAMES libxkomp.so)
find_path(XKOMP_INCLUDE_DIRS NAMES xkomp/xkomp.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(XKOMP DEFAULT_MSG
    XKOMP_LIBRARIES
    XKOMP_INCLUDE_DIRS
)

mark_as_advanced(
    XKOMP_LIBRARIES
    XKOMP_INCLUDE_DIRS
)
