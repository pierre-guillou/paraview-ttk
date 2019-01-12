#
# Find the native Mili includes and library
#
# Mili_INCLUDE_DIR - where to find MILIlib.h, etc.
# Mili_LIBRARIES   - List of fully qualified libraries to link against when using Mili.
# Mili_FOUND       - Do not attempt to use Mili if "no" or undefined.

FIND_PATH(Mili_INCLUDE_DIR MILIlib.h
  /usr/local/include
  /usr/include
)

FIND_LIBRARY(Mili_LIBRARY MILI
  /usr/local/lib
  /usr/lib
)

SET( Mili_FOUND "NO" )
IF(Mili_INCLUDE_DIR)
  IF(Mili_LIBRARY)
    SET( Mili_LIBRARIES ${Mili_LIBRARY} )
    SET( Mili_FOUND "YES" )
  ENDIF(Mili_LIBRARY)
ENDIF(Mili_INCLUDE_DIR)

IF(Mili_FIND_REQUIRED AND NOT Mili_FOUND)
  message(SEND_ERROR "Unable to find the requested Mili libraries.")
ENDIF(Mili_FIND_REQUIRED AND NOT Mili_FOUND)

# handle the QUIETLY and REQUIRED arguments and set Mili_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Mili DEFAULT_MSG Mili_LIBRARY Mili_INCLUDE_DIR)

MARK_AS_ADVANCED(
  Mili_INCLUDE_DIR
  Mili_LIBRARY
)
