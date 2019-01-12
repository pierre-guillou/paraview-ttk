/*=========================================================================

  Program:   ParaView
  Module:    vtkProgressBarSourceRepresentation.h

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkProgressBarSourceRepresentation
 *
 * vtkProgressBarSourceRepresentation is a representation to show ProgressBar. The input is
 * expected to a vtkTable with a single row and column (atleast on the data
 * server nodes). The content of this entry in the table is shown as ProgressBar on the
 * rendering nodes.
*/

#ifndef vtkProgressBarSourceRepresentation_h
#define vtkProgressBarSourceRepresentation_h

#include "vtkPVClientServerCoreRenderingModule.h" //needed for exports
#include "vtkPVDataRepresentation.h"
#include "vtkSmartPointer.h" // for DummyPolyData

class vtk3DWidgetRepresentation;
class vtkPolyData;
class vtkPVCacheKeeper;

class VTKPVCLIENTSERVERCORERENDERING_EXPORT vtkProgressBarSourceRepresentation
  : public vtkPVDataRepresentation
{
public:
  static vtkProgressBarSourceRepresentation* New();
  vtkTypeMacro(vtkProgressBarSourceRepresentation, vtkPVDataRepresentation);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  //@{
  /**
   * Set the ProgressBar widget.
   */
  void SetProgressBarWidgetRepresentation(vtk3DWidgetRepresentation* widget);
  vtkGetObjectMacro(ProgressBarWidgetRepresentation, vtk3DWidgetRepresentation);
  //@}

  void MarkModified() VTK_OVERRIDE;

  /**
   * Set the visibility.
   */
  void SetVisibility(bool) VTK_OVERRIDE;

  /**
   * Set the interactivity.
   */
  void SetInteractivity(bool);

  /**
   * vtkAlgorithm::ProcessRequest() equivalent for rendering passes. This is
   * typically called by the vtkView to request meta-data from the
   * representations or ask them to perform certain tasks e.g.
   * PrepareForRendering.
   */
  int ProcessViewRequest(vtkInformationRequestKey* request_type, vtkInformation* inInfo,
    vtkInformation* outInfo) VTK_OVERRIDE;

  // BTX
protected:
  vtkProgressBarSourceRepresentation();
  ~vtkProgressBarSourceRepresentation() override;

  /**
   * Fill input port information.
   */
  int FillInputPortInformation(int port, vtkInformation* info) VTK_OVERRIDE;

  /**
   * Overridden to invoke vtkCommand::UpdateDataEvent.
   */
  int RequestData(vtkInformation*, vtkInformationVector**, vtkInformationVector*) VTK_OVERRIDE;

  /**
   * Adds the representation to the view.  This is called from
   * vtkView::AddRepresentation().  Subclasses should override this method.
   * Returns true if the addition succeeds.
   */
  bool AddToView(vtkView* view) VTK_OVERRIDE;

  /**
   * Removes the representation to the view.  This is called from
   * vtkView::RemoveRepresentation().  Subclasses should override this method.
   * Returns true if the removal succeeds.
   */
  bool RemoveFromView(vtkView* view) VTK_OVERRIDE;

  /**
   * Overridden to check with the vtkPVCacheKeeper to see if the key is cached.
   */
  bool IsCached(double cache_key) VTK_OVERRIDE;

  vtkPVCacheKeeper* CacheKeeper;
  vtkSmartPointer<vtkPolyData> DummyPolyData;
  vtk3DWidgetRepresentation* ProgressBarWidgetRepresentation;

private:
  vtkProgressBarSourceRepresentation(const vtkProgressBarSourceRepresentation&) = delete;
  void operator=(const vtkProgressBarSourceRepresentation&) = delete;
  // ETX
};

#endif
