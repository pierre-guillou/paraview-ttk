#  Try to find Boxlib library and headers.
#  This file sets the following variables:
#
#  Boxlib_INCLUDE_DIR, where to find BoxLib.H, etc.
#  Boxlib_LIBRARIES, the libraries to link against
#  Boxlib_FOUND, If false, do not try to use Boxlib.

FIND_PATH( Boxlib_INCLUDE_DIR BoxLib.H
  /usr/local/include
  /usr/include
)

FIND_LIBRARY( Boxlib_C_LIBRARY NAMES cboxlib
  /usr/lib
  /usr/local/lib
)

FIND_LIBRARY( Boxlib_Fortran_LIBRARY NAMES fboxlib
  /usr/lib
  /usr/local/lib
)

IF(Boxlib_C_LIBRARY AND Boxlib_Fortran_LIBRARY)
  SET( Boxlib_LIBRARIES ${Boxlib_C_LIBRARY} ${Boxlib_Fortran_LIBRARY})
ENDIF()

# handle the QUIETLY and REQUIRED arguments and set Boxlib_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Boxlib DEFAULT_MSG Boxlib_C_LIBRARY Boxlib_Fortran_LIBRARY Boxlib_INCLUDE_DIR)

MARK_AS_ADVANCED(
  Boxlib_INCLUDE_DIR
  Boxlib_C_LIBRARY
  Boxlib_Fortran_LIBRARY
)
