# Try to find CL headers and libraries.
#
# Usage of this module as follows:
#
#     find_package(CL)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  CL_PREFIX      Set this variable to the root installation of
#                      cuda if the module has problems finding the
#                      proper installation path.
#
# Variables defined by this module:
#
#  CL_FOUND              System has CL libraries and headers
#  CL_LIBRARIES          The CL library for Runtime API
#  CL_INCLUDE_DIRS       The location of CL headers

find_library(CL_LIBRARIES NAMES OpenCL)
find_path(CL_INCLUDE_DIRS NAMES cl.h CL/cl.h include/sycl/CL/cl.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CL DEFAULT_MSG
    CL_LIBRARIES
    CL_INCLUDE_DIRS
)

mark_as_advanced(
    CL_LIBRARIES
    CL_INCLUDE_DIRS
)

