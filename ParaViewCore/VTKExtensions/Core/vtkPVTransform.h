/*=========================================================================

  Program:   ParaView
  Module:    vtkPVTransform

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkPVTransform
 * @brief   baseclass for all ParaView vtkTransform class.
 *
 * vtkPVTransform extend vtkTransform in the sens that it extend the vtkTransform
 * API with absolute position/rotation/scale change and not incremental one like
 * the vtkTransform does.
*/

#ifndef vtkPVTransform_h
#define vtkPVTransform_h

#include "vtkPVVTKExtensionsCoreModule.h" //needed for exports
#include "vtkTransform.h"
class vtkTransform;

class VTKPVVTKEXTENSIONSCORE_EXPORT vtkPVTransform : public vtkTransform
{
public:
  static vtkPVTransform* New();
  vtkTypeMacro(vtkPVTransform, vtkTransform);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  //@{
  /**
   * Get/Set Position of the transform.
   */
  void SetAbsolutePosition(double xyz[3]);
  void SetAbsolutePosition(double x, double y, double z);
  vtkGetVector3Macro(AbsolutePosition, double);
  //@}

  //@{
  /**
   * Get/Set Rotation for the transform.
   */
  void SetAbsoluteRotation(double xyz[3]);
  void SetAbsoluteRotation(double x, double y, double z);
  vtkGetVector3Macro(AbsoluteRotation, double);
  //@}

  //@{
  /**
   * Get/Set Scale for the transform.
   */
  void SetAbsoluteScale(double xyz[3]);
  void SetAbsoluteScale(double x, double y, double z);
  vtkGetVector3Macro(AbsoluteScale, double);
  //@}

protected:
  vtkPVTransform();
  ~vtkPVTransform() override;

  void UpdateMatrix();

  double AbsolutePosition[3];
  double AbsoluteRotation[3];
  double AbsoluteScale[3];
  vtkTransform* AbsoluteTransform;

private:
  vtkPVTransform(const vtkPVTransform&) = delete;
  void operator=(const vtkPVTransform&) = delete;
};

#endif
