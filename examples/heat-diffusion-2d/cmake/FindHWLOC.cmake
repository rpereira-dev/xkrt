# Try to find HWLOC headers and libraries.
#
# Usage of this module as follows:
#
#     find_package(HWLOC)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  HWLOC_PREFIX         Set this variable to the root installation of
#                      libpapi if the module has problems finding the
#                      proper installation path.
#
# Variables defined by this module:
#
#  HWLOC_FOUND              System has HWLOC libraries and headers
#  HWLOC_LIBRARIES          The HWLOC library
#  HWLOC_INCLUDE_DIRS       The location of HWLOC headers

find_library(
    HWLOC_LIBRARIES NAMES libhwloc.so
)

find_path(
    HWLOC_INCLUDE_DIRS NAMES hwloc.h
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(HWLOC DEFAULT_MSG
    HWLOC_LIBRARIES
    HWLOC_INCLUDE_DIRS
)

mark_as_advanced(
    HWLOC_LIBRARIES
    HWLOC_INCLUDE_DIRS
)

