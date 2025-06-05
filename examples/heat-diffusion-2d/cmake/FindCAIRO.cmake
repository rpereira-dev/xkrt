# Try to find CAIRO headers and libraries.
#
# Usage of this module as follows:
#
#     find_package(CAIRO)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  CAIRO_PREFIX      Set this variable to the root installation of
#                      cuda if the module has problems finding the
#                      proper installation path.
#
# Variables defined by this module:
#
#  CAIRO_FOUND              System has CAIRO libraries and headers
#  CAIRO_LIBRARIES          The CAIRO library for Runtime API
#  CAIRO_INCLUDE_DIRS       The location of CAIRO headers

find_library(CAIRO_LIBRARIES NAMES cairo)
find_path(CAIRO_INCLUDE_DIRS NAMES cairo/cairo.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CAIRO DEFAULT_MSG
    CAIRO_LIBRARIES
    CAIRO_INCLUDE_DIRS
)

mark_as_advanced(
    CAIRO_LIBRARIES
    CAIRO_INCLUDE_DIRS
)

