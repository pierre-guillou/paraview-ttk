SET(SOURCES
  avtBoxlib3DFileFormat.C
  BoxLibHelper.C
  )

set(defines)
if (PARAVIEW_USE_MPI)
  list(APPEND defines
    BL_USE_MPI)
endif ()

set_source_files_properties(avtBoxlibFileFormat.C
  COMPILE_DEFINITIONS ${defines})

if(Boxlib_FOUND)
  ADD_VISIT_READER(VisItBoxlib3DReader "1.0"
    VISIT_READER_TYPE "STMD"
    VISIT_READER_NAME "avtBoxlib3DFileFormat"
    VISIT_READER_USES_OPTIONS OFF
    SERVER_SOURCES ${SOURCES}
    )
endif()
