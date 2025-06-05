# Try to find EXCIT headers and libraries.
#
# Usage of this module as follows:
#
#     find_package(EXCIT)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  EXCIT_PREFIX         Set this variable to the root installation of
#                      libpapi if the module has problems finding the
#                      proper installation path.
#
# Variables defined by this module:
#
#  EXCIT_FOUND              System has EXCIT libraries and headers
#  EXCIT_LIBRARIES          The EXCIT library
#  EXCIT_INCLUDE_DIRS       The location of EXCIT headers

find_library(EXCIT_LIBRARIES NAMES libexcit.so)
find_path(EXCIT_INCLUDE_DIRS NAMES excit.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(EXCIT DEFAULT_MSG
    EXCIT_LIBRARIES
    EXCIT_INCLUDE_DIRS
)

mark_as_advanced(
    EXCIT_LIBRARIES
    EXCIT_INCLUDE_DIRS
)
