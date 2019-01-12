/*=========================================================================

  Program:   ParaView
  Module:    vtkSMTextWidgetRepresentationProxy.h

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkSMTextWidgetRepresentationProxy
 *
*/

#ifndef vtkSMTextWidgetRepresentationProxy_h
#define vtkSMTextWidgetRepresentationProxy_h

#include "vtkPVServerManagerRenderingModule.h" //needed for exports
#include "vtkSMNewWidgetRepresentationProxy.h"

class vtkSMViewProxy;

class VTKPVSERVERMANAGERRENDERING_EXPORT vtkSMTextWidgetRepresentationProxy
  : public vtkSMNewWidgetRepresentationProxy
{
public:
  static vtkSMTextWidgetRepresentationProxy* New();
  vtkTypeMacro(vtkSMTextWidgetRepresentationProxy, vtkSMNewWidgetRepresentationProxy);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

protected:
  vtkSMTextWidgetRepresentationProxy();
  ~vtkSMTextWidgetRepresentationProxy() override;

  void CreateVTKObjects() VTK_OVERRIDE;

  vtkSMProxy* TextActorProxy;
  vtkSMProxy* TextPropertyProxy;

  friend class vtkSMTextSourceRepresentationProxy;

private:
  vtkSMTextWidgetRepresentationProxy(const vtkSMTextWidgetRepresentationProxy&) = delete;
  void operator=(const vtkSMTextWidgetRepresentationProxy&) = delete;
};

#endif
