/*=========================================================================

  Program:   ParaView
  Module:    vtkSMExporterProxy.h

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkSMExporterProxy
 * @brief   proxy for view exporters.
 *
 * vtkSMExporterProxy is a proxy for vtkExporter subclasses. It makes it
 * possible to export render views using these exporters.
*/

#ifndef vtkSMExporterProxy_h
#define vtkSMExporterProxy_h

#include "vtkPVServerManagerDefaultModule.h" //needed for exports
#include "vtkSMProxy.h"

#include <string> // For storing file extensions
#include <vector> // For storing file extensions

class vtkSMViewProxy;

class VTKPVSERVERMANAGERDEFAULT_EXPORT vtkSMExporterProxy : public vtkSMProxy
{
public:
  vtkTypeMacro(vtkSMExporterProxy, vtkSMProxy);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  //@{
  /**
   * Set the view proxy to export.
   */
  void SetView(vtkSMViewProxy* view);
  vtkGetObjectMacro(View, vtkSMViewProxy);
  //@}

  /**
   * Exports the view.
   */
  virtual void Write() = 0;

  /**
   * Returns if the view can be exported.
   * Default implementation return true if the view is a render view.
   */
  virtual bool CanExport(vtkSMProxy*) = 0;

  //@{
  /**
   * Returns the suggested file extensions for this exporter.
   */
  const std::vector<std::string>& GetFileExtensions() const { return this->FileExtensions; };
  //@}

  //@{
  /**
   * Returns the suggested file extension for this exporter.
   * @deprecated in ParaView 5.5. Use `GetFileExtensions` instead.
   */
  VTK_LEGACY(const char* GetFileExtension());
  //@}

protected:
  vtkSMExporterProxy();
  ~vtkSMExporterProxy() override;
  /**
   * Read attributes from an XML element.
   */
  int ReadXMLAttributes(vtkSMSessionProxyManager* pm, vtkPVXMLElement* element) VTK_OVERRIDE;

  vtkSMViewProxy* View;
  std::vector<std::string> FileExtensions;

private:
  vtkSMExporterProxy(const vtkSMExporterProxy&) = delete;
  void operator=(const vtkSMExporterProxy&) = delete;
};

#endif
