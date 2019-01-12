set(__dependencies)
if(PARAVIEW_ENABLE_PYTHON)
  list(APPEND __dependencies vtkPVCinemaReader)
endif()

vtk_module(pqApplicationComponents
  GROUPS
    ParaViewQt
  DEPENDS
    pqComponents
    vtkGUISupportQt
  PRIVATE_DEPENDS
    vtkjsoncpp
    vtkPVAnimation
    vtkPVClientServerCoreDefault
    vtkPVServerManagerApplication
    vtkPVServerManagerDefault
    vtkPVServerManagerRendering
    vtksys
    ${__dependencies}
  COMPILE_DEPENDS
    # doesn't really depend on this, but a good way to enable this
    # tool when ParaView UI is being built.
    vtkUtilitiesLegacyColorMapXMLToJSON
  EXCLUDE_FROM_WRAPPING
  TEST_LABELS
    PARAVIEW
)
unset(__dependencies)
