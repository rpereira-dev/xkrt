# Try to find CLBLAST headers and libraries.
#
# Usage of this module as follows:
#
#     find_package(CLBLAST)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  CLBLAST_PREFIX      Set this variable to the root installation of
#                      cuda if the module has problems finding the
#                      proper installation path.
#
# Variables defined by this module:
#
#  CLBLAST_FOUND              System has CLBLAST libraries and headers
#  CLBLAST_LIBRARIES          The CLBLAST library for Runtime API
#  CLBLAST_INCLUDE_DIRS       The location of CLBLAST headers

find_library(CLBLAST_LIBRARIES NAMES clblast)
find_path(CLBLAST_INCLUDE_DIRS NAMES clblast.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CLBLAST DEFAULT_MSG
    CLBLAST_LIBRARIES
    CLBLAST_INCLUDE_DIRS
)

mark_as_advanced(
    CLBLAST_LIBRARIES
    CLBLAST_INCLUDE_DIRS
)

