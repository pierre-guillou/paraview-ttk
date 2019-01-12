/*=========================================================================

  Program:   ParaView
  Module:    vtkSMXYChartRepresentationInitializationHelper.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkSMXYChartRepresentationInitializationHelper.h"

#include "vtkObjectFactory.h"
#include "vtkSMChartSeriesSelectionDomain.h"
#include "vtkSMDomain.h"
#include "vtkSMDomainIterator.h"
#include "vtkSMProperty.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMProxySelectionModel.h"
#include "vtkSMRepresentationProxy.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMViewProxy.h"

#include <cassert>

vtkStandardNewMacro(vtkSMXYChartRepresentationInitializationHelper);
//----------------------------------------------------------------------------
vtkSMXYChartRepresentationInitializationHelper::vtkSMXYChartRepresentationInitializationHelper()
{
}

//----------------------------------------------------------------------------
vtkSMXYChartRepresentationInitializationHelper::~vtkSMXYChartRepresentationInitializationHelper()
{
}

//----------------------------------------------------------------------------
void vtkSMXYChartRepresentationInitializationHelper::PostInitializeProxy(
  vtkSMProxy* proxy, vtkPVXMLElement*, vtkMTimeType vtkNotUsed(ts))
{
  assert(proxy != NULL);

  vtkSMSessionProxyManager* pxm = proxy->GetSessionProxyManager();
  vtkSMViewProxy* activeView = NULL;
  if (vtkSMProxySelectionModel* viewSM = pxm->GetSelectionModel("ActiveView"))
  {
    activeView = vtkSMViewProxy::SafeDownCast(viewSM->GetCurrentProxy());
  }

  vtkSMPropertyHelper input(proxy, "Input");
  if (activeView && (!strcmp(activeView->GetXMLName(), "XYBarChartView") ||
                      !strcmp(activeView->GetXMLName(), "XYHistogramChartView")) &&
    input.GetAsProxy())
  {
    if (vtkSMRepresentationProxy* repr = vtkSMRepresentationProxy::SafeDownCast(proxy))
    {
      if (!strcmp(repr->GetXMLName(), "XYChartRepresentation") &&
        repr->GetProperty("SeriesPlotCorner"))
      {
        vtkSMProperty* corner = repr->GetProperty("SeriesPlotCorner");
        vtkSMChartSeriesSelectionDomain* cornerDomain =
          vtkSMChartSeriesSelectionDomain::SafeDownCast(
            corner->FindDomain("vtkSMChartSeriesSelectionDomain"));
        cornerDomain->SetDefaultValue("2");
      }
    }
  }
}

//----------------------------------------------------------------------------
void vtkSMXYChartRepresentationInitializationHelper::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
