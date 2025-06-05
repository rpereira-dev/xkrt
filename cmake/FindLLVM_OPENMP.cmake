# Try to find LLVM_OPENMP headers and libraries.
#
# Usage of this module as follows:
#
#     find_package(LLVM_OPENMP)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  LLVM_OPENMP_PREFIX         Set this variable to the root installation of
#                      libpapi if the module has problems finding the
#                      proper installation path.
#
# Variables defined by this module:
#
#  LLVM_OPENMP_FOUND              System has LLVM_OPENMP libraries and headers
#  LLVM_OPENMP_LIBRARIES          The LLVM_OPENMP library
#  LLVM_OPENMP_INCLUDE_DIRS       The location of LLVM_OPENMP headers

find_path(LLVM_OPENMP_INCLUDE_DIRS NAMES kmp.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LLVM_OPENMP DEFAULT_MSG
    LLVM_OPENMP_INCLUDE_DIRS
)

mark_as_advanced(
    LLVM_OPENMP_INCLUDE_DIRS
)
