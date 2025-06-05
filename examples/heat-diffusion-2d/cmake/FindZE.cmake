# Try to find ZE headers and libraries.
#
# Usage of this module as follows:
#
#     find_package(ZE)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  ZE_PREFIX      Set this variable to the root installation of
#                      cuda if the module has problems finding the
#                      proper installation path.
#
# Variables defined by this module:
#
#  ZE_FOUND              System has ZE libraries and headers
#  ZE_LIBRARIES          The ZE library for Runtime API
#  ZE_INCLUDE_DIRS       The location of ZE headers

find_library(ZE_LIBRARIES NAMES ze_loader)
find_path(ZE_INCLUDE_DIRS NAMES ze_api.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ZE DEFAULT_MSG
    ZE_LIBRARIES
    ZE_INCLUDE_DIRS
)

mark_as_advanced(
    ZE_LIBRARIES
    ZE_INCLUDE_DIRS
)
