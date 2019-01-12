/*=========================================================================

   Program: ParaView
   Module: pqDoubleVectorPropertyWidget.cxx

   Copyright (c) 2005-2012 Sandia Corporation, Kitware Inc.
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

=========================================================================*/
#include "pqDoubleVectorPropertyWidget.h"

#include "pqCoreUtilities.h"
#include "pqDiscreteDoubleWidget.h"
#include "pqDoubleLineEdit.h"
#include "pqDoubleRangeWidget.h"
#include "pqHighlightableToolButton.h"
#include "pqLabel.h"
#include "pqPropertiesPanel.h"
#include "pqScalarValueListPropertyWidget.h"
#include "pqScaleByButton.h"
#include "pqSignalAdaptors.h"
#include "pqWidgetRangeDomain.h"
#include "vtkCollection.h"
#include "vtkCommand.h"
#include "vtkPVXMLElement.h"
#include "vtkSMArrayRangeDomain.h"
#include "vtkSMBoundsDomain.h"
#include "vtkSMDiscreteDoubleDomain.h"
#include "vtkSMDomain.h"
#include "vtkSMDomainIterator.h"
#include "vtkSMDoubleRangeDomain.h"
#include "vtkSMDoubleVectorProperty.h"
#include "vtkSMProperty.h"
#include "vtkSMProxy.h"
#include "vtkSMUncheckedPropertyHelper.h"

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QMenu>
#include <QStyle>
#include <QToolButton>

//-----------------------------------------------------------------------------
pqDoubleVectorPropertyWidget::pqDoubleVectorPropertyWidget(
  vtkSMProperty* smProperty, vtkSMProxy* smProxy, QWidget* parentObject)
  : pqPropertyWidget(smProxy, parentObject)
{
  this->setProperty(smProperty);
  this->setChangeAvailableAsChangeFinished(false);

  vtkSMDoubleVectorProperty* dvp = vtkSMDoubleVectorProperty::SafeDownCast(smProperty);
  if (!dvp)
  {
    return;
  }

  // find the domain
  vtkSMDoubleRangeDomain* defaultDomain = nullptr;

  vtkSMDomain* domain = nullptr;
  vtkSMDomainIterator* domainIter = dvp->NewDomainIterator();
  for (domainIter->Begin(); !domainIter->IsAtEnd(); domainIter->Next())
  {
    domain = domainIter->GetDomain();
  }
  domainIter->Delete();

  if (!domain)
  {
    defaultDomain = vtkSMDoubleRangeDomain::New();
    domain = defaultDomain;
  }

  QHBoxLayout* layoutLocal = new QHBoxLayout;
  layoutLocal->setMargin(0);
  layoutLocal->setSpacing(pqPropertiesPanel::suggestedHorizontalSpacing());

  this->setLayout(layoutLocal);

  // Fill Layout
  vtkPVXMLElement* hints = dvp->GetHints();
  vtkPVXMLElement* showLabels = nullptr;
  if (hints != nullptr)
  {
    showLabels = hints->FindNestedElementByName("ShowComponentLabels");
  }

  int elementCount = dvp->GetNumberOfElements();

  std::vector<const char*> componentLabels(elementCount);
  if (showLabels)
  {
    vtkNew<vtkCollection> elements;
    showLabels->GetElementsByName("ComponentLabel", elements.GetPointer());
    int nbCompLabels = elements->GetNumberOfItems();
    if (elementCount == 0)
    {
      elementCount = nbCompLabels;
      componentLabels.resize(nbCompLabels);
    }
    for (int i = 0; i < nbCompLabels; ++i)
    {
      vtkPVXMLElement* labelElement = vtkPVXMLElement::SafeDownCast(elements->GetItemAsObject(i));
      if (labelElement)
      {
        int component;
        if (labelElement->GetScalarAttribute("component", &component))
        {
          if (component < elementCount)
          {
            componentLabels[component] = labelElement->GetAttributeOrEmpty("label");
          }
        }
      }
    }
  }

  vtkSMDoubleRangeDomain* range = vtkSMDoubleRangeDomain::SafeDownCast(domain);
  if (this->property()->GetRepeatable())
  {
    pqScalarValueListPropertyWidget* widget =
      new pqScalarValueListPropertyWidget(smProperty, this->proxy(), this);
    widget->setObjectName("ScalarValueList");
    widget->setRangeDomain(range);
    this->addPropertyLink(widget, "scalars", SIGNAL(scalarsChanged()), smProperty);
    widget->setShowLabels(showLabels);
    if (showLabels)
    {
      widget->setLabels(componentLabels);
    }

    this->setChangeAvailableAsChangeFinished(true);
    layoutLocal->addWidget(widget);
    this->setShowLabel(showLabels != nullptr);

    if (range)
    {
      PV_DEBUG_PANELS() << "pqScalarValueListPropertyWidget for a repeatable "
                        << "DoubleVectorProperty with a BoundsDomain ("
                        << pqPropertyWidget::getXMLName(range) << ") ";
    }
    else
    {
      PV_DEBUG_PANELS() << "pqScalarValueListPropertyWidget for a repeatable "
                        << "DoubleVectorProperty without a BoundsDomain";
    }
  }
  else if (range)
  {
    if (dvp->GetNumberOfElements() == 1 &&
      ((range->GetMinimumExists(0) && range->GetMaximumExists(0)) ||
          (dvp->FindDomain("vtkSMArrayRangeDomain") != nullptr ||
            dvp->FindDomain("vtkSMBoundsDomain") != nullptr)))
    {
      // bounded ranges are represented with a slider and a spin box
      pqDoubleRangeWidget* widget = new pqDoubleRangeWidget(this);
      widget->setObjectName("DoubleRangeWidget");
      widget->setUseGlobalPrecisionAndNotation(true);
      widget->setMinimum(range->GetMinimum(0));
      widget->setMaximum(range->GetMaximum(0));
      if (range->GetResolutionExists())
      {
        widget->setResolution(range->GetResolution());
      }

      // ensures that the widget's range is updated whenever the domain changes.
      new pqWidgetRangeDomain(widget, "minimum", "maximum", dvp, 0);

      this->addPropertyLink(widget, "value", SIGNAL(valueChanged(double)), smProperty);
      this->connect(widget, SIGNAL(valueEdited(double)), this, SIGNAL(changeFinished()));

      layoutLocal->addWidget(widget, 1);

      PV_DEBUG_PANELS() << "pqDoubleRangeWidget for a DoubleVectorProperty "
                        << "with a single element and a "
                        << "DoubleRangeDomain (" << pqPropertyWidget::getXMLName(range) << ") "
                        << "with a minimum and a maximum";
    }
    else
    {
      // unbounded ranges are represented with a line edit
      if (elementCount == 6)
      {
        QGridLayout* gridLayout = new QGridLayout;
        gridLayout->setHorizontalSpacing(0);
        gridLayout->setVerticalSpacing(2);

        for (int i = 0; i < 3; i++)
        {
          pqDoubleLineEdit* lineEdit = new pqDoubleLineEdit(this);
          lineEdit->setUseGlobalPrecisionAndNotation(true);
          lineEdit->setObjectName(QString("DoubleLineEdit%1").arg(2 * i));
          if (showLabels)
          {
            pqLabel* label = new pqLabel(componentLabels[2 * i], this);
            label->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
            gridLayout->addWidget(label, (i * 2), 0);
            gridLayout->addWidget(lineEdit, (i * 2) + 1, 0);
          }
          else
          {
            gridLayout->addWidget(lineEdit, i, 0);
          }
          this->addPropertyLink(
            lineEdit, "fullPrecisionText", SIGNAL(textChanged(const QString&)), dvp, 2 * i);
          this->connect(lineEdit, SIGNAL(fullPrecisionTextChangedAndEditingFinished()), this,
            SIGNAL(changeFinished()));

          lineEdit = new pqDoubleLineEdit(this);
          lineEdit->setObjectName(QString("DoubleLineEdit%1").arg(2 * i + 1));
          lineEdit->setUseGlobalPrecisionAndNotation(true);
          if (showLabels)
          {
            pqLabel* label = new pqLabel(componentLabels[2 * i + 1], this);
            label->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
            gridLayout->addWidget(label, (i * 2), 1);
            gridLayout->addWidget(lineEdit, (i * 2) + 1, 1);
          }
          else
          {
            gridLayout->addWidget(lineEdit, i, 1);
          }
          this->addPropertyLink(
            lineEdit, "fullPrecisionText", SIGNAL(textChanged(const QString&)), dvp, 2 * i + 1);

          this->connect(lineEdit, SIGNAL(fullPrecisionTextChangedAndEditingFinished()), this,
            SIGNAL(changeFinished()));
        }

        layoutLocal->addLayout(gridLayout);

        PV_DEBUG_PANELS() << "3x2 grid of QLineEdit's for an DoubleVectorProperty "
                          << "with an "
                          << "DoubleRangeDomain (" << pqPropertyWidget::getXMLName(range) << ") "
                          << "and 6 elements";
      }
      else
      {
        for (unsigned int i = 0; i < dvp->GetNumberOfElements(); i++)
        {
          if (showLabels)
          {
            pqLabel* label = new pqLabel(componentLabels[i], this);
            label->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
            layoutLocal->addWidget(label);
          }
          pqDoubleLineEdit* lineEdit = new pqDoubleLineEdit(this);
          lineEdit->setObjectName(QString("DoubleLineEdit%1").arg(i));
          lineEdit->setUseGlobalPrecisionAndNotation(true);
          layoutLocal->addWidget(lineEdit);
          this->addPropertyLink(
            lineEdit, "fullPrecisionText", SIGNAL(textChanged(const QString&)), dvp, i);

          this->connect(lineEdit, SIGNAL(fullPrecisionTextChangedAndEditingFinished()), this,
            SIGNAL(changeFinished()));
        }

        PV_DEBUG_PANELS() << "List of QLineEdit's for an DoubleVectorProperty "
                          << "with an "
                          << "DoubleRangeDomain (" << pqPropertyWidget::getXMLName(range) << ") "
                          << "and more than one element";
      }
    }
  }
  else if (vtkSMDiscreteDoubleDomain* discrete = vtkSMDiscreteDoubleDomain::SafeDownCast(domain))
  {
    if (discrete->GetValuesExists())
    {
      pqDiscreteDoubleWidget* widget = new pqDiscreteDoubleWidget(this);
      widget->setObjectName("DiscreteDoubleWidget");
      widget->setUseGlobalPrecisionAndNotation(true);
      widget->setValues(discrete->GetValues());

      this->addPropertyLink(widget, "value", SIGNAL(valueChanged(double)), smProperty);
      this->connect(widget, SIGNAL(valueEdited(double)), this, SIGNAL(changeFinished()));

      layoutLocal->addWidget(widget);

      PV_DEBUG_PANELS() << "pqDiscreteDoubleWidget for an DoubleVectorProperty "
                        << "with a single element and a DiscreteDoubleDomain"
                        << " (" << pqPropertyWidget::getXMLName(discrete) << ") "
                        << "with a set of values";
    }
    else
    {
      qCritical("vtkSMDiscreteDoubleDomain does not contain any value.");
    }
  }

  if (dvp->FindDomain("vtkSMArrayRangeDomain") != nullptr ||
    dvp->FindDomain("vtkSMBoundsDomain") != nullptr)
  {
    PV_DEBUG_PANELS() << "Adding \"Scale\" button since the domain is dynamically";
    pqScaleByButton* scaleButton = new pqScaleByButton(this);
    scaleButton->setObjectName("ScaleBy");
    this->connect(scaleButton, SIGNAL(scale(double)), SLOT(scale(double)));
    layoutLocal->addWidget(scaleButton, 0, Qt::AlignBottom);

    PV_DEBUG_PANELS() << "Adding \"Reset\" button since the domain is dynamically";

    // if this has an vtkSMArrayRangeDomain, add a "reset" button.
    pqHighlightableToolButton* resetButton = new pqHighlightableToolButton(this);
    resetButton->setObjectName("Reset");
    QAction* resetActn = new QAction(resetButton);
    resetActn->setToolTip("Reset using current data values");
    resetActn->setIcon(resetButton->style()->standardIcon(QStyle::SP_BrowserReload));
    resetButton->addAction(resetActn);
    resetButton->setDefaultAction(resetActn);

    pqCoreUtilities::connect(
      dvp, vtkCommand::DomainModifiedEvent, this, SIGNAL(highlightResetButton()));
    pqCoreUtilities::connect(
      dvp, vtkCommand::UncheckedPropertyModifiedEvent, this, SIGNAL(highlightResetButton()));

    this->connect(resetButton, SIGNAL(clicked()), SLOT(resetButtonClicked()));
    resetButton->connect(this, SIGNAL(highlightResetButton()), SLOT(highlight()));
    resetButton->connect(this, SIGNAL(clearHighlight()), SLOT(clear()));

    layoutLocal->addWidget(resetButton, 0, Qt::AlignBottom);
  }
  if (defaultDomain)
  {
    defaultDomain->Delete();
  }
}

//-----------------------------------------------------------------------------
pqDoubleVectorPropertyWidget::~pqDoubleVectorPropertyWidget()
{
}

//-----------------------------------------------------------------------------
void pqDoubleVectorPropertyWidget::resetButtonClicked()
{
  if (vtkSMProperty* smproperty = this->property())
  {
    smproperty->ResetToDomainDefaults(/*use_unchecked_values*/ true);
    emit this->changeAvailable();
    emit this->changeFinished();
  }
  emit this->clearHighlight();
}

//-----------------------------------------------------------------------------
void pqDoubleVectorPropertyWidget::apply()
{
  this->Superclass::apply();
  emit this->clearHighlight();
}

//-----------------------------------------------------------------------------
void pqDoubleVectorPropertyWidget::reset()
{
  this->Superclass::reset();
  emit this->clearHighlight();
}

//-----------------------------------------------------------------------------
void pqDoubleVectorPropertyWidget::scaleHalf()
{
  this->scale(0.5);
}

//-----------------------------------------------------------------------------
void pqDoubleVectorPropertyWidget::scaleTwice()
{
  this->scale(2.0);
}

//-----------------------------------------------------------------------------
void pqDoubleVectorPropertyWidget::scale(double factor)
{
  if (vtkSMProperty* smproperty = this->property())
  {
    vtkSMUncheckedPropertyHelper helper(smproperty);
    for (unsigned int cc = 0, max = helper.GetNumberOfElements(); cc < max; cc++)
    {
      helper.Set(cc, helper.GetAsDouble(cc) * factor);
    }
    emit this->changeAvailable();
    emit this->changeFinished();
  }
}
