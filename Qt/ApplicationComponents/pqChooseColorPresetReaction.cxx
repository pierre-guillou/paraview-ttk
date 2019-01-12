/*=========================================================================

   Program: ParaView
   Module:  pqChooseColorPresetReaction.cxx

   Copyright (c) 2005,2006 Sandia Corporation, Kitware Inc.
   All rights reserved.

   ParaView is a free software; you can redistribute it and/or modify it
   under the terms of the ParaView license version 1.2.

   See License_v1.2.txt for the full ParaView license.
   A copy of this license can be obtained by contacting
   Kitware Inc.
   28 Corporate Drive
   Clifton Park, NY 12065
   USA

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

========================================================================*/
#include "pqChooseColorPresetReaction.h"

#include "pqActiveObjects.h"
#include "pqCoreUtilities.h"
#include "pqDataRepresentation.h"
#include "pqPresetDialog.h"
#include "pqUndoStack.h"
#include "vtkSMPVRepresentationProxy.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMTransferFunctionProxy.h"

QPointer<pqPresetDialog> pqChooseColorPresetReaction::PresetDialog;

namespace pvInternals
{
vtkSMProxy* lutProxy(vtkSMProxy* reprProxy)
{
  if (vtkSMPVRepresentationProxy::GetUsingScalarColoring(reprProxy))
  {
    return vtkSMPropertyHelper(reprProxy, "LookupTable", true).GetAsProxy();
  }
  return NULL;
}
}

//-----------------------------------------------------------------------------
pqChooseColorPresetReaction::pqChooseColorPresetReaction(
  QAction* parentObject, bool track_active_objects)
  : Superclass(parentObject)
{
  if (track_active_objects)
  {
    QObject::connect(&pqActiveObjects::instance(),
      SIGNAL(representationChanged(pqDataRepresentation*)), this,
      SLOT(setRepresentation(pqDataRepresentation*)));
    this->setRepresentation(pqActiveObjects::instance().activeRepresentation());
  }
}

//-----------------------------------------------------------------------------
pqChooseColorPresetReaction::~pqChooseColorPresetReaction()
{
}

//-----------------------------------------------------------------------------
void pqChooseColorPresetReaction::setRepresentation(pqDataRepresentation* repr)
{
  if (this->Representation == repr)
  {
    return;
  }
  if (this->Representation)
  {
    this->disconnect(this->Representation);
  }
  this->Representation = repr;
  if (repr)
  {
    this->connect(repr, SIGNAL(colorTransferFunctionModified()), SLOT(updateTransferFunction()));
    this->connect(repr, SIGNAL(colorArrayNameModified()), SLOT(updateTransferFunction()));
  }
  this->updateTransferFunction();
}

//-----------------------------------------------------------------------------
void pqChooseColorPresetReaction::updateTransferFunction()
{
  this->setTransferFunction(
    this->Representation ? this->Representation->getLookupTableProxy() : NULL);
}

//-----------------------------------------------------------------------------
void pqChooseColorPresetReaction::setTransferFunction(vtkSMProxy* lut)
{
  this->TransferFunctionProxy = lut;
  this->updateEnableState();
}

//-----------------------------------------------------------------------------
void pqChooseColorPresetReaction::updateEnableState()
{
  this->parentAction()->setEnabled(this->TransferFunctionProxy != NULL);
}

//-----------------------------------------------------------------------------
void pqChooseColorPresetReaction::onTriggered()
{
  this->choosePreset();
}

//-----------------------------------------------------------------------------
bool pqChooseColorPresetReaction::choosePreset(const char* presetName)
{
  vtkSMProxy* lut = this->TransferFunctionProxy;
  if (!lut)
  {
    return false;
  }

  bool indexedLookup = vtkSMPropertyHelper(lut, "IndexedLookup", true).GetAsInt() != 0;

  if (PresetDialog)
  {
    PresetDialog->setMode(indexedLookup ? pqPresetDialog::SHOW_INDEXED_COLORS_ONLY
                                        : pqPresetDialog::SHOW_NON_INDEXED_COLORS_ONLY);
  }
  else
  {
    // This should be deleted when the mainWidget is closed and it should be impossible
    // to get back here with the preset dialog open due to the event filtering done by
    // the preset dialog.
    PresetDialog = new pqPresetDialog(pqCoreUtilities::mainWidget(), indexedLookup
        ? pqPresetDialog::SHOW_INDEXED_COLORS_ONLY
        : pqPresetDialog::SHOW_NON_INDEXED_COLORS_ONLY);
  }

  PresetDialog->setCurrentPreset(presetName);
  PresetDialog->setCustomizableLoadColors(!indexedLookup);
  PresetDialog->setCustomizableLoadOpacities(!indexedLookup);
  PresetDialog->setCustomizableUsePresetRange(!indexedLookup);
  PresetDialog->setCustomizableLoadAnnotations(indexedLookup);
  this->connect(PresetDialog, SIGNAL(applyPreset(const Json::Value&)), SLOT(applyCurrentPreset()));
  PresetDialog->show();
  return true;
}

//-----------------------------------------------------------------------------
void pqChooseColorPresetReaction::applyCurrentPreset()
{
  pqPresetDialog* dialog = qobject_cast<pqPresetDialog*>(this->sender());
  Q_ASSERT(dialog);
  Q_ASSERT(dialog == PresetDialog);

  vtkSMProxy* lut = this->TransferFunctionProxy;
  if (!lut)
  {
    return;
  }

  BEGIN_UNDO_SET("Apply color preset");
  if (dialog->loadColors() || dialog->loadOpacities())
  {
    vtkSMProxy* sof = vtkSMPropertyHelper(lut, "ScalarOpacityFunction", true).GetAsProxy();
    if (dialog->loadColors())
    {
      vtkSMTransferFunctionProxy::ApplyPreset(
        lut, dialog->currentPreset(), !dialog->usePresetRange());
    }
    if (dialog->loadOpacities())
    {
      if (sof)
      {
        vtkSMTransferFunctionProxy::ApplyPreset(
          sof, dialog->currentPreset(), !dialog->usePresetRange());
      }
      else
      {
        qWarning("Cannot load opacities since 'ScalarOpacityFunction' is not present.");
      }
    }

    // We need to take extra care to avoid the color and opacity function ranges
    // from straying away from each other. This can happen if only one of them is
    // getting a preset and we're using the preset range.
    if (dialog->usePresetRange() && (dialog->loadColors() ^ dialog->loadOpacities()) && sof)
    {
      double range[2];
      if (dialog->loadColors() && vtkSMTransferFunctionProxy::GetRange(lut, range))
      {
        vtkSMTransferFunctionProxy::RescaleTransferFunction(sof, range);
      }
      else if (dialog->loadOpacities() && vtkSMTransferFunctionProxy::GetRange(sof, range))
      {
        vtkSMTransferFunctionProxy::RescaleTransferFunction(lut, range);
      }
    }
  }
  if (dialog->loadAnnotations())
  {
    vtkSMTransferFunctionProxy::ApplyPreset(
      lut, dialog->currentPreset(), !dialog->loadAnnotations());
  }
  END_UNDO_SET();
  emit this->presetApplied();
}
