// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @class   vtkSMCameraProxy
 * @brief   proxy for a camera.
 *
 * This a proxy for a vtkCamera. This class optimizes UpdatePropertyInformation
 * to use the client side object.
 */

#ifndef vtkSMCameraProxy_h
#define vtkSMCameraProxy_h

#include "vtkRemotingViewsModule.h" //needed for exports
#include "vtkSMProxy.h"

class VTKREMOTINGVIEWS_EXPORT vtkSMCameraProxy : public vtkSMProxy
{
public:
  static vtkSMCameraProxy* New();
  vtkTypeMacro(vtkSMCameraProxy, vtkSMProxy);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  ///@{
  /**
   * Updates all property information by calling UpdateInformation()
   * and populating the values.
   */
  void UpdatePropertyInformation() override;
  void UpdatePropertyInformation(vtkSMProperty* prop) override
  {
    this->Superclass::UpdatePropertyInformation(prop);
  }

protected:
  vtkSMCameraProxy();
  ~vtkSMCameraProxy() override;
  ///@}

private:
  vtkSMCameraProxy(const vtkSMCameraProxy&) = delete;
  void operator=(const vtkSMCameraProxy&) = delete;
};

#endif
