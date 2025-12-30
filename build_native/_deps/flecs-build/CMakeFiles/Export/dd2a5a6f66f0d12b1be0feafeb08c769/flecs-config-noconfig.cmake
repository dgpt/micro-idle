#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "flecs::flecs" for configuration ""
set_property(TARGET flecs::flecs APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(flecs::flecs PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libflecs.so"
  IMPORTED_SONAME_NOCONFIG "libflecs.so"
  )

list(APPEND _cmake_import_check_targets flecs::flecs )
list(APPEND _cmake_import_check_files_for_flecs::flecs "${_IMPORT_PREFIX}/lib/libflecs.so" )

# Import target "flecs::flecs_static" for configuration ""
set_property(TARGET flecs::flecs_static APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(flecs::flecs_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "C"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libflecs_static.a"
  )

list(APPEND _cmake_import_check_targets flecs::flecs_static )
list(APPEND _cmake_import_check_files_for_flecs::flecs_static "${_IMPORT_PREFIX}/lib/libflecs_static.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
