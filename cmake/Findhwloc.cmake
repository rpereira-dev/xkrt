# Try to find hwloc library and headers
# Once done, this will define:
#
#   HWLOC_FOUND          - TRUE if hwloc was found
#   HWLOC_INCLUDE_DIRS   - include directories
#   HWLOC_LIBRARIES      - libraries to link
#
# and an imported target:
#   hwloc::hwloc
#

find_path(HWLOC_INCLUDE_DIR
  NAMES hwloc.h
  PATHS
    /usr/include
    /usr/local/include
    /opt/local/include
)

find_library(HWLOC_LIBRARY
  NAMES hwloc
  PATHS
    /usr/lib
    /usr/lib64
    /usr/local/lib
    /usr/local/lib64
    /opt/local/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(hwloc
  REQUIRED_VARS HWLOC_LIBRARY HWLOC_INCLUDE_DIR
  FAIL_MESSAGE "Could not find hwloc library"
)

if(HWLOC_FOUND)
  set(HWLOC_LIBRARIES ${HWLOC_LIBRARY})
  set(HWLOC_INCLUDE_DIRS ${HWLOC_INCLUDE_DIR})

  if(NOT TARGET hwloc::hwloc)
    add_library(hwloc::hwloc UNKNOWN IMPORTED)
    set_target_properties(hwloc::hwloc PROPERTIES
      IMPORTED_LOCATION "${HWLOC_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${HWLOC_INCLUDE_DIR}"
    )
  endif()
endif()

