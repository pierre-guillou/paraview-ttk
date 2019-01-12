vtk_module(vtkIOVisItBridge
  DEPENDS
    vtkCommonDataModel
    vtkCommonExecutionModel
    vtkFiltersAMR
    vtkParallelCore
  PRIVATE_DEPENDS
    VisItLib
    vtkCommonSystem
    vtkhdf5
    vtkIOImage
    vtknetcdf
    vtknetcdfcpp
    vtkRendering${VTK_RENDERING_BACKEND}
    vtksys
    vtkzlib
  TEST_DEPENDS
)

# paraview-sepecfic extensions to a module to bring in proxy xmls.
set_property (GLOBAL PROPERTY
  vtkIOVisItBridge_SERVERMANAGER_XMLS
  ${CMAKE_CURRENT_LIST_DIR}/visit_readers.xml)
