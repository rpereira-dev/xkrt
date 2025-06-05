# Try to find LLVM_OFFLOAD headers and libraries.
#
# Usage of this module as follows:
#
#     find_package(LLVM_OFFLOAD)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  LLVM_OFFLOAD_PREFIX         Set this variable to the root installation of
#                      libpapi if the module has problems finding the
#                      proper installation path.
#
# Variables defined by this module:
#
#  LLVM_OFFLOAD_FOUND              System has LLVM_OFFLOAD libraries and headers
#  LLVM_OFFLOAD_INCLUDE_DIRS       The location of LLVM_OFFLOAD headers

find_path(LLVM_OFFLOAD_INCLUDE_DIRS NAMES offload/include/omptarget.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LLVM_OFFLOAD DEFAULT_MSG
    LLVM_OFFLOAD_INCLUDE_DIRS
)

mark_as_advanced(
    LLVM_OFFLOAD_INCLUDE_DIRS
)
