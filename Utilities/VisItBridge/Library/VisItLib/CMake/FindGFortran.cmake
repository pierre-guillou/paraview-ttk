#  Try to find GFortran libraries.
#  This file sets the following variables:
#
#  GFORTRAN_LIBRARIES, the libraries to link against
#  GFORTRAN_FOUND, If false, do not try to use GFORTRAN

set(_gfortran_extra_paths)
if (CMAKE_Fortran_COMPILER AND CMAKE_Fortran_COMPILER_ID STREQUAL "GNU")
  get_filename_component(_gfortran_root "${CMAKE_Fortran_COMPILER}" DIRECTORY)
  get_filename_component(_gfortran_root "${_gfortran_root}" DIRECTORY)
  list(APPEND _gfortran_extra_paths
    "${_gfortran_root}/lib")
  message("${_gfortran_extra_paths}")
endif ()

find_library(gfortran_LIBRARY NAMES gfortran
  HINTS
  ${_gfortran_extra_paths}
  PATHS
  /usr/lib
  /usr/local/lib
  PATH_SUFFIXES
  gcc/x86_64-linux-gnu/${CMAKE_Fortran_COMPILER_VERSION}/
  gcc/x86_64-redhat-linux/${CMAKE_Fortran_COMPILER_VERSION}/
  )
if (gfortran_LIBRARY)
  set(GFortran_LIBRARIES ${gfortran_LIBRARY})
endif()
find_library(quadmath_LIBRARY NAMES quadmath
  HINTS
  ${_gfortran_extra_paths}
  PATHS
  /usr/lib
  /usr/local/lib
  PATH_SUFFIXES
  gcc/x86_64-linux-gnu/${CMAKE_Fortran_COMPILER_VERSION}/
  gcc/x86_64-redhat-linux/${CMAKE_Fortran_COMPILER_VERSION}/
  )
if (quadmath_LIBRARY)
  list(APPEND GFortran_LIBRARIES ${quadmath_LIBRARY})
endif()

# handle the QUIETLY and REQUIRED arguments and set GFortran_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GFortran DEFAULT_MSG gfortran_LIBRARY quadmath_LIBRARY)

MARK_AS_ADVANCED(
  gfortran_LIBRARY
  quadmath_LIBRARY
)
