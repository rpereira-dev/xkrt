# Try to find LLVM headers and libraries.
#
# Usage of this module as follows:
#
#     find_package(LLVM)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  LLVM_PREFIX         Set this variable to the root installation of
#                      libpapi if the module has problems finding the
#                      proper installation path.
#
# Variables defined by this module:
#
#  LLVM_FOUND              System has LLVM libraries and headers
#  LLVM_INCLUDE_DIRS       The location of LLVM headers

find_path(LLVM_INCLUDE_DIRS NAMES llvm/include/llvm/Frontend/OpenMP/OMPDeviceConstants.h)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LLVM DEFAULT_MSG LLVM_INCLUDE_DIRS)
mark_as_advanced(LLVM_INCLUDE_DIRS)
