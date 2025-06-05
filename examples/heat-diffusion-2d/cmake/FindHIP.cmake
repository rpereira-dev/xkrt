# Try to find HIP headers and libraries.
#
# Usage of this module as follows:
#
#     find_package(HIPLocal)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  HIP_PREFIX          Set this variable to the root installation of
#                      libamdhip64.so if the module has problems finding the
#                      proper installation path.
#
# Variables defined by this module:
#
#  HIP_FOUND              System has HIP libraries and headers
#  HIP_LIBRARIES          The HIP library
#  HIP_INCLUDE_DIRS       The location of HIP headers

find_path(HIP_PREFIX
    NAMES include/hip/hip_runtime.h
    HINTS $ENV{HIP_PATH}
)

set(HIP_LIBRARIES "")
foreach(libname
    amdhip64
    rocm_smi64
    amd_smi
    rocm_smi
)
    unset(_FOUND_LIB CACHE)
    find_library(_FOUND_LIB
        NAMES ${libname}
        HINTS ${HIP_PREFIX}/lib ${HILTIDEPS}/lib
    )
    if(_FOUND_LIB)
        list(APPEND HIP_LIBRARIES ${_FOUND_LIB})
    endif()
endforeach()

find_path(HIP_INCLUDE_DIRS
    NAMES hip/hip_runtime.h
    HINTS ${HIP_PREFIX}/include ${HILTIDEPS}/include
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(HIP DEFAULT_MSG
    HIP_LIBRARIES
    HIP_INCLUDE_DIRS
)

mark_as_advanced(
    HIP_PREFIX_DIRS
    HIP_LIBRARIES
    HIP_INCLUDE_DIRS
)

