# Try to find NVML headers and libraries.
#
# Usage of this module as follows:
#
#     find_package(NVML)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  NVML_PREFIX      Set this variable to the root installation of
#                     cuda if the module has problems finding the
#                     proper installation path.
#
# Variables defined by this module:
#
#  NVML_FOUND              System has NVML libraries and headers
#  NVML_LIBRARIES          The NVML library for Runtime API
#  NVML_INCLUDE_DIRS       The location of NVML headers

find_library(NVML_LIBRARIES NAMES nvidia-ml)
find_path(NVML_INCLUDE_DIRS NAMES nvml.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NVML DEFAULT_MSG
    NVML_LIBRARIES
    NVML_INCLUDE_DIRS
)

mark_as_advanced(
    NVML_LIBRARIES
    NVML_INCLUDE_DIRS
)
