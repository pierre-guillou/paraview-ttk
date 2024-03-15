// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-FileCopyrightText: Copyright (c) Sandia Corporation
// SPDX-License-Identifier: BSD-3-Clause
#include "pqMenuReactionUtils.h"

#include "vtkSMDataTypeDomain.h"
#include "vtkSMInputArrayDomain.h"
#include "vtkSMInputProperty.h"
#include "vtkSMPropertyIterator.h"
#include "vtkSMProxy.h"

#include <QStringList>
#include <vector>

namespace pqMenuReactionUtils
{

vtkSMInputProperty* getInputProperty(vtkSMProxy* proxy)
{
  // if "Input" is present, we return that, otherwise the "first"
  // vtkSMInputProperty encountered is returned.

  vtkSMInputProperty* prop = vtkSMInputProperty::SafeDownCast(proxy->GetProperty("Input"));
  vtkSMPropertyIterator* propIter = proxy->NewPropertyIterator();
  for (propIter->Begin(); !prop && !propIter->IsAtEnd(); propIter->Next())
  {
    prop = vtkSMInputProperty::SafeDownCast(propIter->GetProperty());
  }

  propIter->Delete();
  return prop;
}

QString getDomainDisplayText(vtkSMDomain* domain)
{
  if (auto dtd = vtkSMDataTypeDomain::SafeDownCast(domain))
  {
    return QString(dtd->GetDomainDescription().c_str());
  }
  else if (domain->IsA("vtkSMInputArrayDomain"))
  {
    vtkSMInputArrayDomain* iad = static_cast<vtkSMInputArrayDomain*>(domain);
    QString txt = (iad->GetAttributeType() == vtkSMInputArrayDomain::ANY
        ? QString("Requires an attribute array")
        : QString("Requires a %1 attribute array").arg(iad->GetAttributeTypeAsString()));
    std::vector<int> numbersOfComponents = iad->GetAcceptableNumbersOfComponents();
    if (!numbersOfComponents.empty())
    {
      txt += QString(" with ");
      for (unsigned int i = 0; i < numbersOfComponents.size(); i++)
      {
        if (i == numbersOfComponents.size() - 1)
        {
          txt += QString("%1 ").arg(numbersOfComponents[i]);
        }
        else
        {
          txt += QString("%1 or ").arg(numbersOfComponents[i]);
        }
      }
      txt += QString("component(s)");
    }
    return txt;
  }
  return QString("Requirements not met");
}
}
