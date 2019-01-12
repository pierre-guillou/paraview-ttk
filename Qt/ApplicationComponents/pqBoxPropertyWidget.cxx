/*=========================================================================

   Program: ParaView
   Module:  pqBoxPropertyWidget.cxx

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
#include "pqBoxPropertyWidget.h"
#include "ui_pqBoxPropertyWidget.h"

#include "vtkSMNewWidgetRepresentationProxy.h"
#include "vtkSMPropertyGroup.h"
#include "vtkSMPropertyHelper.h"

//-----------------------------------------------------------------------------
pqBoxPropertyWidget::pqBoxPropertyWidget(
  vtkSMProxy* smproxy, vtkSMPropertyGroup* smgroup, QWidget* parentObject)
  : Superclass("representations", "BoxWidgetRepresentation", smproxy, smgroup, parentObject)
{
  Ui::BoxPropertyWidget ui;
  ui.setupUi(this);

  vtkSMProxy* wdgProxy = this->widgetProxy();

  ui.translateX->setToolTip("hi");
  // Let's link some of the UI elements that only affect the interactive widget
  // properties without affecting properties on the main proxy.
  this->WidgetLinks.addPropertyLink(ui.enableTranslation, "checked", SIGNAL(toggled(bool)),
    wdgProxy, wdgProxy->GetProperty("TranslationEnabled"));
  this->WidgetLinks.addPropertyLink(ui.enableScaling, "checked", SIGNAL(toggled(bool)), wdgProxy,
    wdgProxy->GetProperty("ScalingEnabled"));
  this->WidgetLinks.addPropertyLink(ui.enableRotation, "checked", SIGNAL(toggled(bool)), wdgProxy,
    wdgProxy->GetProperty("RotationEnabled"));
  this->WidgetLinks.addPropertyLink(ui.enableMoveFaces, "checked", SIGNAL(toggled(bool)), wdgProxy,
    wdgProxy->GetProperty("MoveFacesEnabled"));

  if (vtkSMProperty* position = smgroup->GetProperty("Position"))
  {
    this->addPropertyLink(ui.translateX, "fullPrecisionText",
      SIGNAL(fullPrecisionTextChangedAndEditingFinished()), position, 0);
    this->addPropertyLink(ui.translateY, "fullPrecisionText",
      SIGNAL(fullPrecisionTextChangedAndEditingFinished()), position, 1);
    this->addPropertyLink(ui.translateZ, "fullPrecisionText",
      SIGNAL(fullPrecisionTextChangedAndEditingFinished()), position, 2);
    ui.labelTranslate->setText(position->GetXMLLabel());
    QString tooltip = this->getTooltip(position);
    ui.translateX->setToolTip(tooltip);
    ui.translateY->setToolTip(tooltip);
    ui.translateZ->setToolTip(tooltip);
    ui.labelTranslate->setToolTip(tooltip);
  }
  else
  {
    ui.labelTranslate->hide();
    ui.translateX->hide();
    ui.translateY->hide();
    ui.translateZ->hide();

    // see WidgetLinks above.
    ui.enableTranslation->setChecked(false);
    ui.enableTranslation->hide();
  }

  if (vtkSMProperty* rotation = smgroup->GetProperty("Rotation"))
  {
    this->addPropertyLink(ui.rotateX, "fullPrecisionText",
      SIGNAL(fullPrecisionTextChangedAndEditingFinished()), rotation, 0);
    this->addPropertyLink(ui.rotateY, "fullPrecisionText",
      SIGNAL(fullPrecisionTextChangedAndEditingFinished()), rotation, 1);
    this->addPropertyLink(ui.rotateZ, "fullPrecisionText",
      SIGNAL(fullPrecisionTextChangedAndEditingFinished()), rotation, 2);
    ui.labelRotate->setText(rotation->GetXMLLabel());
    QString tooltip = this->getTooltip(rotation);
    ui.rotateX->setToolTip(tooltip);
    ui.rotateY->setToolTip(tooltip);
    ui.rotateZ->setToolTip(tooltip);
    ui.labelRotate->setToolTip(tooltip);
  }
  else
  {
    ui.labelRotate->hide();
    ui.rotateX->hide();
    ui.rotateY->hide();
    ui.rotateZ->hide();

    // see WidgetLinks above.
    ui.enableRotation->setChecked(false);
    ui.enableRotation->hide();
  }

  if (vtkSMProperty* scale = smgroup->GetProperty("Scale"))
  {
    this->addPropertyLink(ui.scaleX, "fullPrecisionText",
      SIGNAL(fullPrecisionTextChangedAndEditingFinished()), scale, 0);
    this->addPropertyLink(ui.scaleY, "fullPrecisionText",
      SIGNAL(fullPrecisionTextChangedAndEditingFinished()), scale, 1);
    this->addPropertyLink(ui.scaleZ, "fullPrecisionText",
      SIGNAL(fullPrecisionTextChangedAndEditingFinished()), scale, 2);
    ui.labelScale->setText(scale->GetXMLLabel());
    QString tooltip = this->getTooltip(scale);
    ui.scaleX->setToolTip(tooltip);
    ui.scaleY->setToolTip(tooltip);
    ui.scaleZ->setToolTip(tooltip);
    ui.labelScale->setToolTip(tooltip);
  }
  else
  {
    ui.labelScale->hide();
    ui.scaleX->hide();
    ui.scaleY->hide();
    ui.scaleZ->hide();

    // see WidgetLinks above.
    ui.enableScaling->setChecked(false);
    ui.enableScaling->hide();
    ui.enableMoveFaces->setChecked(false);
    ui.enableMoveFaces->hide();
  }

  this->connect(&this->WidgetLinks, SIGNAL(qtWidgetChanged()), SLOT(render()));

  // link show3DWidget checkbox
  this->connect(ui.show3DWidget, SIGNAL(toggled(bool)), SLOT(setWidgetVisible(bool)));
  ui.show3DWidget->connect(this, SIGNAL(widgetVisibilityToggled(bool)), SLOT(setChecked(bool)));
  this->setWidgetVisible(ui.show3DWidget->isChecked());

  // hiding this since this is not connected to anything currently. Need to
  // figure out what exactly should it do.
  ui.resetBounds->hide();
}

//-----------------------------------------------------------------------------
pqBoxPropertyWidget::~pqBoxPropertyWidget()
{
}

//-----------------------------------------------------------------------------
void pqBoxPropertyWidget::placeWidget()
{
  vtkBoundingBox bbox = this->dataBounds();
  if (!bbox.IsValid())
  {
    bbox = vtkBoundingBox(0, 1, 0, 1, 0, 1);
  }

  vtkSMNewWidgetRepresentationProxy* wdgProxy = this->widgetProxy();

  double bds[6];
  bbox.GetBounds(bds);
  vtkSMPropertyHelper(wdgProxy, "PlaceWidget").Set(bds, 6);
  wdgProxy->UpdateVTKObjects();

  // This is incorrect. We should never be changing properties on the proxy like
  // this. The way we are letting users set the box without explicitly setting
  // the bounds is wrong. We'll have revisit that. For now, I am letting this
  // be. Please don't follow this pattern in your code.
  vtkSMPropertyHelper(this->proxy(), "Bounds", /*quiet*/ true).Set(bds, 6);
  this->proxy()->UpdateVTKObjects();
}
