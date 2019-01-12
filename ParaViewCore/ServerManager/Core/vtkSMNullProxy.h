/*=========================================================================

  Program:   ParaView
  Module:    vtkSMNullProxy.h

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkSMNullProxy
 * @brief   proxy with stands for NULL object on the server.
 *
 * vtkSMNullProxy stands for a 0 on the server side.
*/

#ifndef vtkSMNullProxy_h
#define vtkSMNullProxy_h

#include "vtkPVServerManagerCoreModule.h" //needed for exports
#include "vtkSMProxy.h"

class VTKPVSERVERMANAGERCORE_EXPORT vtkSMNullProxy : public vtkSMProxy
{
public:
  static vtkSMNullProxy* New();
  vtkTypeMacro(vtkSMNullProxy, vtkSMProxy);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

protected:
  vtkSMNullProxy();
  ~vtkSMNullProxy() override;

  void CreateVTKObjects() VTK_OVERRIDE;

private:
  vtkSMNullProxy(const vtkSMNullProxy&) = delete;
  void operator=(const vtkSMNullProxy&) = delete;
};

#endif
