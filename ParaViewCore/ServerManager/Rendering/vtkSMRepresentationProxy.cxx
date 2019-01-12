/*=========================================================================

  Program:   ParaView
  Module:    $RCSfile$

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkSMRepresentationProxy.h"

#include "vtkClientServerStream.h"
#include "vtkCommand.h"
#include "vtkDataObject.h"
#include "vtkObjectFactory.h"
#include "vtkPVProminentValuesInformation.h"
#include "vtkPVRepresentedDataInformation.h"
#include "vtkSMInputProperty.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMProxyInternals.h"
#include "vtkSMSession.h"
#include "vtkSMStringListDomain.h"
#include "vtkSMTrace.h"
#include "vtkTimerLog.h"

#include <cassert>
#include <sstream>

#define MAX_NUMBER_OF_INTERNAL_REPRESENTATIONS 10

vtkStandardNewMacro(vtkSMRepresentationProxy);
//----------------------------------------------------------------------------
vtkSMRepresentationProxy::vtkSMRepresentationProxy()
{
  this->SetExecutiveName("vtkPVDataRepresentationPipeline");
  this->RepresentedDataInformationValid = false;
  this->RepresentedDataInformation = vtkPVRepresentedDataInformation::New();
  this->ProminentValuesInformation = vtkPVProminentValuesInformation::New();
  this->ProminentValuesFraction = -1;
  this->ProminentValuesUncertainty = -1;
  this->ProminentValuesInformationValid = false;

  this->MarkedModified = false;
  this->VTKRepresentationUpdated = false;
}

//----------------------------------------------------------------------------
vtkSMRepresentationProxy::~vtkSMRepresentationProxy()
{
  this->RepresentedDataInformation->Delete();
  this->ProminentValuesInformation->Delete();
}

//----------------------------------------------------------------------------
void vtkSMRepresentationProxy::SetDebugName(const char* name)
{
  if (this->ObjectsCreated)
  {
    vtkErrorMacro("`SetDebugName` cannot be called after `CreateVTKObjects`.");
  }
  else if (name != nullptr && name[0] != '\0')
  {
    this->DebugName = name;
    for (unsigned int cc = 0, max = this->GetNumberOfSubProxies(); cc < max; ++cc)
    {
      if (auto subrepr = vtkSMRepresentationProxy::SafeDownCast(this->GetSubProxy(cc)))
      {
        std::ostringstream str;
        str << this->DebugName << "/" << this->GetSubProxyName(cc);
        subrepr->SetDebugName(str.str().c_str());
      }
    }
  }
}

//----------------------------------------------------------------------------
const char* vtkSMRepresentationProxy::GetDebugName() const
{
  return this->DebugName.empty() ? nullptr : this->DebugName.c_str();
}

//----------------------------------------------------------------------------
void vtkSMRepresentationProxy::CreateVTKObjects()
{
  if (this->ObjectsCreated)
  {
    return;
  }

  this->Superclass::CreateVTKObjects();

  // If prototype, no need to add listeners...
  if (this->Location == 0 || !this->ObjectsCreated)
  {
    return;
  }

  // Initialize vtkPVDataRepresentation with a unique ID
  vtkClientServerStream stream;
  stream << vtkClientServerStream::Invoke << VTKOBJECT(this) << "Initialize"
         << static_cast<unsigned int>(this->GetGlobalID())
         << static_cast<unsigned int>(this->GetGlobalID() + MAX_NUMBER_OF_INTERNAL_REPRESENTATIONS)
         << vtkClientServerStream::End;
  if (!this->DebugName.empty())
  {
    stream << vtkClientServerStream::Invoke << VTKOBJECT(this) << "SetDebugName"
           << this->DebugName.c_str() << vtkClientServerStream::End;
  }
  this->ExecuteStream(stream);

  if (auto obj = vtkObject::SafeDownCast(this->GetClientSideObject()))
  {
    obj->AddObserver(
      vtkCommand::UpdateDataEvent, this, &vtkSMRepresentationProxy::OnVTKRepresentationUpdated);
  }
}

//---------------------------------------------------------------------------
int vtkSMRepresentationProxy::LoadXMLState(
  vtkPVXMLElement* proxyElement, vtkSMProxyLocator* locator)
{
  vtkTypeUInt32 oldserver = this->Location;
  int ret = this->Superclass::LoadXMLState(proxyElement, locator);
  this->Location = oldserver;
  return ret;
}

//----------------------------------------------------------------------------
void vtkSMRepresentationProxy::AddConsumer(vtkSMProperty* property, vtkSMProxy* proxy)
{
  this->Superclass::AddConsumer(property, proxy);
  for (unsigned int cc = 0; cc < this->GetNumberOfSubProxies(); cc++)
  {
    vtkSMRepresentationProxy* repr = vtkSMRepresentationProxy::SafeDownCast(this->GetSubProxy(cc));
    if (repr)
    {
      repr->AddConsumer(property, proxy);
    }
  }
}

//----------------------------------------------------------------------------
void vtkSMRepresentationProxy::RemoveConsumer(vtkSMProperty* property, vtkSMProxy* proxy)
{
  this->Superclass::RemoveConsumer(property, proxy);
  for (unsigned int cc = 0; cc < this->GetNumberOfSubProxies(); cc++)
  {
    vtkSMRepresentationProxy* repr = vtkSMRepresentationProxy::SafeDownCast(this->GetSubProxy(cc));
    if (repr)
    {
      repr->RemoveConsumer(property, proxy);
    }
  }
}

//----------------------------------------------------------------------------
void vtkSMRepresentationProxy::RemoveAllConsumers()
{
  this->Superclass::RemoveAllConsumers();
  for (unsigned int cc = 0; cc < this->GetNumberOfSubProxies(); cc++)
  {
    vtkSMRepresentationProxy* repr = vtkSMRepresentationProxy::SafeDownCast(this->GetSubProxy(cc));
    if (repr)
    {
      repr->RemoveAllConsumers();
    }
  }
}

//----------------------------------------------------------------------------
void vtkSMRepresentationProxy::UpdatePipeline()
{
  if (!this->NeedsUpdate)
  {
    return;
  }

  this->UpdatePipelineInternal(0, false);
  this->Superclass::UpdatePipeline();
}

//----------------------------------------------------------------------------
void vtkSMRepresentationProxy::UpdatePipeline(double time)
{
  this->UpdatePipelineInternal(time, true);
  this->Superclass::UpdatePipeline();
}

//----------------------------------------------------------------------------
void vtkSMRepresentationProxy::UpdatePipelineInternal(double time, bool doTime)
{
  vtkClientServerStream stream;

  if (doTime)
  {
    stream << vtkClientServerStream::Invoke << VTKOBJECT(this) << "UpdateTimeStep" << time
           << vtkClientServerStream::End;
  }
  else
  {
    stream << vtkClientServerStream::Invoke << VTKOBJECT(this) << "Update"
           << vtkClientServerStream::End;
  }

  this->GetSession()->PrepareProgress();
  this->ExecuteStream(stream);
  this->GetSession()->CleanupPendingProgress();
}

//----------------------------------------------------------------------------
void vtkSMRepresentationProxy::MarkDirtyFromProducer(
  vtkSMProxy* modifiedProxy, vtkSMProxy* producer, vtkSMProperty* property)
{
  assert(producer != this);
  if (this->ObjectsCreated && !this->MarkedModified)
  {
    // `producer` has been "modified". Now the question to answer is if that
    // modification of the `producer` is enough for this representation to re-execute
    // i.e. generate new geometry (or any other appropriate artifact) for rendering
    // and re-deliver to the rendering nodes, clear caches etc. Alternatively, it
    // could merely be a "rendering" change, e.g. change in LUT, that doesn't
    // require us to re-execute the representation.
    //
    // To answer that question, we rely on following observation:
    // Typically a producer is marked as such because of a ProxyProperty or
    // InputProperty. If it's a ProxyProperty, it's not a pipeline connection
    // and hence changing it should not affect representation's data processing
    // pipeline (only rendering pipeline).
    //
    // Of course, there may be exceptions....TODO:
    if (vtkSMInputProperty::SafeDownCast(property) != nullptr || property == nullptr)
    {
      this->MarkedModified = true;
      this->VTKRepresentationUpdated = false;
      vtkClientServerStream stream;
      stream << vtkClientServerStream::Invoke << VTKOBJECT(this) << "MarkModified"
             << vtkClientServerStream::End;
      this->ExecuteStream(stream);
    }
  }

  this->Superclass::MarkDirtyFromProducer(modifiedProxy, producer, property);
}

//----------------------------------------------------------------------------
void vtkSMRepresentationProxy::MarkDirty(vtkSMProxy* modifiedProxy)
{
  // vtkSMProxy::MarkDirty does not call MarkConsumersAsDirty unless
  // this->NeedsUpdate is false. Generally, that's indeed correct since we
  // have marked the consumer dirty previously, we don't need to do it again.
  // However since consumers of representations are generally views, they need
  // to marked dirty everytime (otherwise unhiding a representation would not
  // result in the view realizing that data may have changed). Hence we force
  // NeedsUpdate to false.
  this->NeedsUpdate = false;
  this->Superclass::MarkDirty(modifiedProxy);
}

//----------------------------------------------------------------------------
void vtkSMRepresentationProxy::OnVTKRepresentationUpdated()
{
  this->MarkedModified = false;
  this->VTKRepresentationUpdated = true;
}

//----------------------------------------------------------------------------
void vtkSMRepresentationProxy::ViewUpdated(vtkSMProxy* view)
{
  this->PostUpdateData();

  // If this class has sub-representations, we need to tell those that the view
  // has updated as well.
  for (unsigned int cc = 0; cc < this->GetNumberOfSubProxies(); cc++)
  {
    vtkSMRepresentationProxy* repr = vtkSMRepresentationProxy::SafeDownCast(this->GetSubProxy(cc));
    if (repr)
    {
      repr->ViewUpdated(view);
    }
  }
}

//----------------------------------------------------------------------------
void vtkSMRepresentationProxy::PostUpdateData()
{
  // PostUpdateData may get called on all representations on the client side
  // whenever the view updates. However, the underlying vtkPVDataRepresentation
  // object may not have updated (possibly because of visibility being false).
  // In that case, we should not let PostUpdateData() happen. The following
  // check ensures that PostUpdateData() call has any effect only after the VTK
  // representation has updated as well.
  if (this->MarkedModified == false && this->VTKRepresentationUpdated == true)
  {
    this->Superclass::PostUpdateData();
  }
}

//----------------------------------------------------------------------------
void vtkSMRepresentationProxy::InvalidateDataInformation()
{
  this->Superclass::InvalidateDataInformation();
  this->RepresentedDataInformationValid = false;
  this->ProminentValuesInformationValid = false;
}

//----------------------------------------------------------------------------
vtkPVDataInformation* vtkSMRepresentationProxy::GetRepresentedDataInformation()
{
  if (!this->RepresentedDataInformationValid)
  {
    vtkTimerLog::MarkStartEvent("vtkSMRepresentationProxy::GetRepresentedDataInformation");
    this->RepresentedDataInformation->Initialize();
    this->GatherInformation(this->RepresentedDataInformation);
    vtkTimerLog::MarkEndEvent("vtkSMRepresentationProxy::GetRepresentedDataInformation");
    this->RepresentedDataInformationValid = true;
  }

  return this->RepresentedDataInformation;
}

//----------------------------------------------------------------------------
vtkPVProminentValuesInformation* vtkSMRepresentationProxy::GetProminentValuesInformation(
  vtkStdString name, int fieldAssoc, int numComponents, double uncertaintyAllowed, double fraction,
  bool force)
{
  bool differentAttribute =
    this->ProminentValuesInformation->GetNumberOfComponents() != numComponents ||
    this->ProminentValuesInformation->GetFieldName() != name ||
    strcmp(this->ProminentValuesInformation->GetFieldAssociation(),
      vtkDataObject::GetAssociationTypeAsString(fieldAssoc));
  bool invalid = this->ProminentValuesFraction < 0. || this->ProminentValuesUncertainty < 0. ||
    this->ProminentValuesFraction > 1. || this->ProminentValuesUncertainty > 1.;
  bool largerFractionOrLessCertain = this->ProminentValuesFraction < fraction ||
    this->ProminentValuesUncertainty > uncertaintyAllowed;
  if (!this->ProminentValuesInformationValid || differentAttribute || invalid ||
    largerFractionOrLessCertain || force)
  {
    vtkTimerLog::MarkStartEvent("vtkSMRepresentationProxy::GetProminentValues");
    this->CreateVTKObjects();

    // Initialize parameters with specified values:
    this->ProminentValuesInformation->Initialize();
    this->ProminentValuesInformation->SetFieldAssociation(
      vtkDataObject::GetAssociationTypeAsString(fieldAssoc));
    this->ProminentValuesInformation->SetFieldName(name);
    this->ProminentValuesInformation->SetNumberOfComponents(numComponents);
    this->ProminentValuesInformation->SetUncertainty(uncertaintyAllowed);
    this->ProminentValuesInformation->SetFraction(fraction);
    this->ProminentValuesInformation->SetForce(force);

    // Ask the server to fill out the rest of the information:

    // Now, we need to check if the array of interest is on the input or
    // produced as an artifact by the representation.
    vtkSMPropertyHelper inputHelper(this, "Input");
    vtkSMSourceProxy* input = vtkSMSourceProxy::SafeDownCast(inputHelper.GetAsProxy());
    const unsigned int port = inputHelper.GetOutputPort();
    if (input &&
      input->GetDataInformation(port)->GetArrayInformation(name.c_str(), fieldAssoc) != nullptr)
    {
      this->ProminentValuesInformation->SetPortNumber(port);
      input->GatherInformation(this->ProminentValuesInformation);
    }
    else
    {
      this->GatherInformation(this->ProminentValuesInformation);
    }

    vtkTimerLog::MarkEndEvent("vtkSMRepresentationProxy::GetProminentValues");
    this->ProminentValuesFraction = fraction;
    this->ProminentValuesUncertainty = uncertaintyAllowed;
    this->ProminentValuesInformationValid = true;
  }

  return this->ProminentValuesInformation;
}

//-----------------------------------------------------------------------------
void vtkSMRepresentationProxy::ViewTimeChanged()
{
  vtkSMProxy* current = this;
  vtkSMProxyProperty* pp = vtkSMProxyProperty::SafeDownCast(current->GetProperty("Input"));
  while (current && pp && pp->GetNumberOfProxies() > 0)
  {
    current = pp->GetProxy(0);
    pp = vtkSMProxyProperty::SafeDownCast(current->GetProperty("Input"));
  }

  if (current)
  {
    current->MarkModified(current);
  }
}
//----------------------------------------------------------------------------
void vtkSMRepresentationProxy::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//---------------------------------------------------------------------------
vtkTypeUInt32 vtkSMRepresentationProxy::GetGlobalID()
{
  bool has_gid = this->HasGlobalID();

  if (!has_gid && this->Session != NULL)
  {
    // reserve 1+MAX_NUMBER_OF_INTERNAL_REPRESENTATIONS contiguous IDs for the source proxies and
    // possible extract
    // selection proxies.
    this->SetGlobalID(this->GetSession()->GetNextChunkGlobalUniqueIdentifier(
      1 + MAX_NUMBER_OF_INTERNAL_REPRESENTATIONS));
  }
  return this->GlobalID;
}

//---------------------------------------------------------------------------
bool vtkSMRepresentationProxy::SetRepresentationType(const char* type)
{
  if (vtkSMProperty* property = this->GetProperty("Representation"))
  {
    vtkSMStringListDomain* sld =
      vtkSMStringListDomain::SafeDownCast(property->FindDomain("vtkSMStringListDomain"));

    unsigned int tmp;
    if (sld != NULL && sld->IsInDomain(type, tmp) == 0)
    {
      // Let's not warn about this. Let the caller decide if this is an
      // error/warning.
      // vtkWarningMacro("Requested type not available: " << type);
      return false;
    }

    SM_SCOPED_TRACE(CallMethod)
      .arg(this)
      .arg("SetRepresentationType")
      .arg(type)
      .arg("comment", "change representation type");

    vtkSMPropertyHelper(property).Set(type ? type : "");
    this->UpdateVTKObjects();
    return true;
  }

  return false;
}
