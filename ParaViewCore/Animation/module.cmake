# This module collects all animation support classes in ParaView,
# both VTK extensions and proxies.

# If FFMPEG support is enabled, we need to depend on FFMPEG.
set (__extra_dependencies)
if (PARAVIEW_ENABLE_FFMPEG)
  list(APPEND __extra_dependencies vtkIOFFMPEG)
endif()

vtk_module(vtkPVAnimation
  DEPENDS
    vtkPVServerManagerCore
    vtkPVVTKExtensionsDefault
  PRIVATE_DEPENDS
    vtksys
    vtkIOMovie
    vtkPVServerManagerDefault
    ${__extra_dependencies}
  TEST_LABELS
    PARAVIEW
)
unset(__extra_dependencies)

# Add proxy definitions.
set_property(GLOBAL PROPERTY
  vtkPVAnimation_SERVERMANAGER_XMLS ${CMAKE_CURRENT_LIST_DIR}/animation.xml)

if(WIN32)
  set_property(GLOBAL  APPEND PROPERTY
    vtkPVAnimation_SERVERMANAGER_XMLS
    ${CMAKE_CURRENT_LIST_DIR}/animation_win.xml)
endif()

if(PARAVIEW_ENABLE_FFMPEG)
  set_property(GLOBAL  APPEND PROPERTY
    vtkPVAnimation_SERVERMANAGER_XMLS
    ${CMAKE_CURRENT_LIST_DIR}/animation_ffmpeg.xml)
endif()


if((NOT CMAKE_C_COMPILER_ID STREQUAL "XL") AND (NOT ${CMAKE_CROSSCOMPILING}))
  # see vtkIOMovie's module.cmake
  # after new modularization changes, this will get cleaner
  # as we'll look for a target.
  set_property(GLOBAL  APPEND PROPERTY
    vtkPVAnimation_SERVERMANAGER_XMLS
    ${CMAKE_CURRENT_LIST_DIR}/animation_oggtheora.xml)
endif()
