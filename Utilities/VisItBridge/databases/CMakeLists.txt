#get same cmake config as visitlib itself does
include(${CMAKE_CURRENT_SOURCE_DIR}/../Library/VisItLib/CMake/VisItCommon.cmake)
include(${VISIT_BINARY_DIR}/VisItBridgeUse.cmake)

macro(VISIT_INCLUDE_READER_SOURCE name state)
option(VISIT_BUILD_READER_${name} "Build VisIt Reader ${name}" ${state})
if (VISIT_BUILD_READER_${name})
  add_subdirectory(readers/${name})
endif()
endmacro(VISIT_INCLUDE_READER_SOURCE name)

#these are used by visit_include_reader
set(VISIT_DATABASE_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(VISIT_SERVER_SOURCES CACHE INTERNAL "vtk classes to wrap for client server" FORCE)
set(VISIT_DB_SOURCES "" CACHE INTERNAL "visit sources for readers" FORCE)
set(VISIT_DB_INC_DIRS CACHE INTERNAL "include directories" FORCE)

set(DEFAULT_BRIDGE_READERS
  ANSYS
  AUXFile
  BOV
  CEAucd
  Chombo
  CMAT
  CTRL
  Curve2D
  DDCMD
  Dyna3D
  ExtrudedVol
  FLASH
  Fluent
  Gadget
  GGCM
  GTC
  GULP
  H5Nimrod
  Image
  LAMMPS
  Lines
  M3D
  M3DC1
  MFIX
  MFIXCDF
  Miranda
  MM5
  NASTRAN
  NETCDF
  Nek5000
  OpenFOAM
  OVERFLOW
  paraDIS
  paraDIS_tecplot
  PATRAN
  PFLOTRAN
  Pixie
  PLOT3D
  Point3D
  ProteinDataBank
  RAW
  SAMRAI
  SAR
  SAS
  Spheral
  Tecplot
  Tetrad
  TFT
  TSurf
  UNIC
  VASP
  Velodyne
  Vs
  Xmdv
  XYZ
  )
if(NOT WIN32)
  list(APPEND DEFAULT_BRIDGE_READERS
    Claw
    )
endif(NOT WIN32)

#add generic vtkreader interface parent classes
add_subdirectory(AvtAlgorithms)

#add the default readers to the library
foreach(db ${DEFAULT_BRIDGE_READERS})
  add_subdirectory(readers/${db})
endforeach(db)

add_definitions("-DRC_CPP_VISIT_BUILD")


#-----------------------------------------------------------------------------
visit_include_reader_source(Silo OFF)
if(VISIT_BUILD_READER_Silo)
  if(NOT HAVE_LIBSILO)
    #if we currently don't have silo setup make it required
    find_package(SILO REQUIRED)
  endif(NOT HAVE_LIBSILO)
  if(NOT SILO_FOUND)
    message(FATAL_ERROR "Unable to locate Silo library.")
  endif(NOT SILO_FOUND)
endif(VISIT_BUILD_READER_Silo)

#-----------------------------------------------------------------------------
if(VISIT_BUILD_READER_Boxlib3D)
  if(NOT Boxlib_FOUND)
    find_package(Boxlib REQUIRED)
  endif(NOT Boxlib_FOUND)
  if(NOT GFortran_FOUND)
    find_package(GFortran REQUIRED)
  endif()
  if(NOT MPI_CXX_FOUND)
    find_package(MPI REQUIRED)
  endif()
endif(VISIT_BUILD_READER_Boxlib3D)
visit_include_reader_source(Boxlib3D OFF)

#-----------------------------------------------------------------------------
visit_include_reader_source(Mili OFF)
if(VISIT_BUILD_READER_Mili)
  if(NOT HAVE_LIBMILI)
    #if we currently don't have silo setup make it required
    find_package(Mili REQUIRED)
  endif(NOT HAVE_LIBMILI)
  if(NOT Mili_FOUND)
    message(FATAL_ERROR "Unable to locate Mili library.")
  endif(NOT Mili_FOUND)
endif(VISIT_BUILD_READER_Mili)

#-----------------------------------------------------------------------------
#include all the db directories before
#the visit files
include_directories(BEFORE
  ${VISIT_DB_INC_DIRS}
  )

include_directories(
  ${VISITBRIDGE_INCLUDE_DIRS}
  )

if(HAVE_LIBSILO AND VISIT_BUILD_READER_Silo)
  include_directories(${SILO_INCLUDE_DIR})
endif(HAVE_LIBSILO AND VISIT_BUILD_READER_Silo)

if(Boxlib_FOUND AND VISIT_BUILD_READER_Boxlib3D)
  include_directories(${Boxlib_INCLUDE_DIR})
endif(Boxlib_FOUND AND VISIT_BUILD_READER_Boxlib3D)

if(HAVE_LIBMILI AND VISIT_BUILD_READER_Mili)
  include_directories(${Mili_INCLUDE_DIR})
endif(HAVE_LIBMILI AND VISIT_BUILD_READER_Mili)

set(Module_SRCS
  ${VISIT_SERVER_SOURCES}
  )

set_source_files_properties(
  ${VISIT_DB_SOURCES}
  PROPERTIES
    WRAP_EXCLUDE 1
    WRAP_EXCLUDE_PYTHON 1
    SKIP_HEADER_INSTALL 1)

set(vtkIOVisItBridge_INCLUDE_DIRS "${vtkIOVisItBridge_INCLUDE_DIRS};${CMAKE_CURRENT_SOURCE_DIR}/AvtAlgorithms")
vtk_module_library(vtkIOVisItBridge ${Module_SRCS} ${VISIT_DB_SOURCES})

# if you want to add vtk-module dependencies, simply add those in module.cmake,
# rather than explicitly adding those by using target_link_libraries.

if(VISIT_PARALLEL)
   target_link_libraries(vtkIOVisItBridge
        LINK_PRIVATE
          visitcommon avtdatabase_par avtpipeline_par
          visit_vtk)
else(VISIT_PARALLEL)
  target_link_libraries(vtkIOVisItBridge
        LINK_PRIVATE
          visitcommon avtdatabase_ser avtpipeline_ser
          visit_vtk)
endif(VISIT_PARALLEL)

if(HAVE_LIBSILO AND VISIT_BUILD_READER_Silo)
  target_link_libraries(vtkIOVisItBridge LINK_PRIVATE ${SILO_LIBRARIES})
endif(HAVE_LIBSILO AND VISIT_BUILD_READER_Silo)

if(Boxlib_FOUND AND VISIT_BUILD_READER_Boxlib3D)
  target_link_libraries(vtkIOVisItBridge LINK_PRIVATE ${Boxlib_LIBRARIES})
  target_link_libraries(vtkIOVisItBridge LINK_PRIVATE ${GFortran_LIBRARIES})
  if(VISIT_PARALLEL)
    target_link_libraries(vtkIOVisItBridge LINK_PRIVATE ${MPI_CXX_LIBRARIES} ${MPI_Fortran_LIBRARIES})
  endif()
endif(Boxlib_FOUND AND VISIT_BUILD_READER_Boxlib3D)

if(HAVE_LIBMILI AND VISIT_BUILD_READER_Mili)
  target_link_libraries(vtkIOVisItBridge LINK_PRIVATE ${Mili_LIBRARIES})
endif(HAVE_LIBMILI AND VISIT_BUILD_READER_Mili)
