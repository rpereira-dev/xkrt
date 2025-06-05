# Try to find HSA headers and libraries.
#
# Usage of this module as follows:
#
#     find_package(HSALocal)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  HSA_PREFIX          Set this variable to the root installation of
#                      libamdhip64.so if the module has problems finding the
#                      proper installation path.
#
# Variables defined by this module:
#
#  HSA_FOUND              System has HSA libraries and headers
#  HSA_LIBRARIES          The HSA library
#  HSA_INCLUDE_DIRS       The location of HSA headers

find_path(HSA_PREFIX
    NAMES include/hip/hip_runtime.h
    HINTS $ENV{HSA_PATH}
)

find_library(HSA_LIBRARIES
    # Pick the static library first for easier run-time linking.
    NAMES libamdhip64.so librocm_smi64.so libamd_smi.so librocm_smi.so
    HINTS ${HSA_PREFIX}/lib ${HILTIDEPS}/lib
)

find_path(HSA_INCLUDE_DIRS
    NAMES hip/hip_runtime.h
    HINTS ${HSA_PREFIX}/include ${HILTIDEPS}/include
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(HSA DEFAULT_MSG
    HSA_LIBRARIES
    HSA_INCLUDE_DIRS
)

mark_as_advanced(
    HSA_PREFIX_DIRS
    HSA_LIBRARIES
    HSA_INCLUDE_DIRS
)

