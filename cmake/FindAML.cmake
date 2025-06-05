# Try to find AML headers and libraries.
#
# Usage of this module as follows:
#
#     find_package(AML)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  AML_PREFIX         Set this variable to the root installation of
#                      libpapi if the module has problems finding the
#                      proper installation path.
#
# Variables defined by this module:
#
#  AML_FOUND              System has AML libraries and headers
#  AML_LIBRARIES          The AML library
#  AML_INCLUDE_DIRS       The location of AML headers

find_library(AML_LIBRARIES NAMES libaml.so)
find_path(AML_INCLUDE_DIRS NAMES aml.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(AML DEFAULT_MSG
    AML_LIBRARIES
    AML_INCLUDE_DIRS
)

mark_as_advanced(
    AML_LIBRARIES
    AML_INCLUDE_DIRS
)
