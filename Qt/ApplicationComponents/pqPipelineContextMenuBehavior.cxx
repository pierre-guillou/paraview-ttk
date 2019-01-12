/*=========================================================================

   Program: ParaView
   Module:    $RCSfile$

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
#include "pqPipelineContextMenuBehavior.h"

#include "pqActiveObjects.h"
#include "pqApplicationCore.h"
#include "pqCoreUtilities.h"
#include "pqDoubleRangeDialog.h"
#include "pqEditColorMapReaction.h"
#include "pqPVApplicationCore.h"
#include "pqPipelineRepresentation.h"
#include "pqRenderView.h"
#include "pqSMAdaptor.h"
#include "pqScalarsToColors.h"
#include "pqSelectionManager.h"
#include "pqServerManagerModel.h"
#include "pqSetName.h"
#include "pqUndoStack.h"
#include "vtkDataObject.h"
#include "vtkNew.h"
#include "vtkPVCompositeDataInformation.h"
#include "vtkPVDataInformation.h"
#include "vtkPVGeneralSettings.h"
#include "vtkSMArrayListDomain.h"
#include "vtkSMDoubleMapProperty.h"
#include "vtkSMDoubleMapPropertyIterator.h"
#include "vtkSMIntVectorProperty.h"
#include "vtkSMPVRepresentationProxy.h"
#include "vtkSMProperty.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMTransferFunctionManager.h"
#include "vtkSMViewProxy.h"

#include <QAction>
#include <QApplication>
#include <QColorDialog>
#include <QMenu>
#include <QMouseEvent>
#include <QPair>
#include <QWidget>

namespace
{
// converts array association/name pair to QVariant.
QVariant convert(const QPair<int, QString>& array)
{
  if (!array.second.isEmpty())
  {
    QStringList val;
    val << QString::number(array.first) << array.second;
    return val;
  }
  return QVariant();
}

// converts QVariant to array association/name pair.
QPair<int, QString> convert(const QVariant& val)
{
  QPair<int, QString> result;
  if (val.canConvert<QStringList>())
  {
    QStringList list = val.toStringList();
    Q_ASSERT(list.size() == 2);
    result.first = list[0].toInt();
    result.second = list[1];
  }
  return result;
}
}

//-----------------------------------------------------------------------------
pqPipelineContextMenuBehavior::pqPipelineContextMenuBehavior(QObject* parentObject)
  : Superclass(parentObject)
{
  QObject::connect(pqApplicationCore::instance()->getServerManagerModel(),
    SIGNAL(viewAdded(pqView*)), this, SLOT(onViewAdded(pqView*)));
  this->Menu = new QMenu();
  this->Menu << pqSetName("PipelineContextMenu");
}

//-----------------------------------------------------------------------------
pqPipelineContextMenuBehavior::~pqPipelineContextMenuBehavior()
{
  delete this->Menu;
}

//-----------------------------------------------------------------------------
void pqPipelineContextMenuBehavior::onViewAdded(pqView* view)
{
  if (view && view->getProxy()->IsA("vtkSMRenderViewProxy"))
  {
    // add a link view menu
    view->widget()->installEventFilter(this);
  }
}

//-----------------------------------------------------------------------------
bool pqPipelineContextMenuBehavior::eventFilter(QObject* caller, QEvent* e)
{
  if (e->type() == QEvent::MouseButtonPress)
  {
    QMouseEvent* me = static_cast<QMouseEvent*>(e);
    if (me->button() & Qt::RightButton)
    {
      this->Position = me->pos();
    }
  }
  else if (e->type() == QEvent::MouseButtonRelease)
  {
    QMouseEvent* me = static_cast<QMouseEvent*>(e);
    if (me->button() & Qt::RightButton && !this->Position.isNull())
    {
      QPoint newPos = static_cast<QMouseEvent*>(e)->pos();
      QPoint delta = newPos - this->Position;
      QWidget* senderWidget = qobject_cast<QWidget*>(caller);
      if (delta.manhattanLength() < 3 && senderWidget != NULL)
      {
        pqRenderView* view = qobject_cast<pqRenderView*>(pqActiveObjects::instance().activeView());
        if (view)
        {
          int pos[2] = { newPos.x(), newPos.y() };
          // we need to flip Y.
          int height = senderWidget->size().height();
          pos[1] = height - pos[1];
          unsigned int blockIndex = 0;
          this->PickedRepresentation = view->pickBlock(pos, blockIndex);
          this->buildMenu(this->PickedRepresentation, blockIndex);
          this->Menu->popup(senderWidget->mapToGlobal(newPos));
        }
      }
      this->Position = QPoint();
    }
  }

  return Superclass::eventFilter(caller, e);
}

//-----------------------------------------------------------------------------
void pqPipelineContextMenuBehavior::buildMenu(pqDataRepresentation* repr, unsigned int blockIndex)
{
  pqRenderView* view = qobject_cast<pqRenderView*>(pqActiveObjects::instance().activeView());

  // get currently selected block ids
  this->PickedBlocks.clear();

  bool picked_block_in_selected_blocks = false;
  pqSelectionManager* selectionManager = pqPVApplicationCore::instance()->selectionManager();
  if (selectionManager)
  {
    pqOutputPort* port = selectionManager->getSelectedPort();
    if (port)
    {
      vtkSMSourceProxy* activeSelection = port->getSelectionInput();
      if (activeSelection && strcmp(activeSelection->GetXMLName(), "BlockSelectionSource") == 0)
      {
        vtkSMPropertyHelper blocksProp(activeSelection, "Blocks");
        QVector<vtkIdType> vblocks;
        vblocks.resize(blocksProp.GetNumberOfElements());
        blocksProp.Get(&vblocks[0], blocksProp.GetNumberOfElements());
        foreach (const vtkIdType& index, vblocks)
        {
          if (index >= 0)
          {
            if (static_cast<unsigned int>(index) == blockIndex)
            {
              picked_block_in_selected_blocks = true;
            }
            this->PickedBlocks.push_back(static_cast<unsigned int>(index));
          }
        }
      }
    }
  }

  if (!picked_block_in_selected_blocks)
  {
    // the block that was clicked on is not one of the currently selected
    // block so actions should only affect that block
    this->PickedBlocks.clear();
    this->PickedBlocks.append(static_cast<unsigned int>(blockIndex));
  }

  this->Menu->clear();
  if (repr)
  {
    vtkPVDataInformation* info = repr->getInputDataInformation();
    vtkPVCompositeDataInformation* compositeInfo = info->GetCompositeDataInformation();
    if (compositeInfo && compositeInfo->GetDataIsComposite())
    {
      bool multipleBlocks = this->PickedBlocks.size() > 1;

      if (multipleBlocks)
      {
        this->Menu->addAction(QString("%1 Blocks").arg(this->PickedBlocks.size()));
      }
      else
      {
        QString blockName = this->lookupBlockName(blockIndex);
        this->Menu->addAction(QString("Block '%1'").arg(blockName));
      }
      this->Menu->addSeparator();

      QAction* hideBlockAction =
        this->Menu->addAction(QString("Hide Block%1").arg(multipleBlocks ? "s" : ""));
      this->connect(hideBlockAction, SIGNAL(triggered()), this, SLOT(hideBlock()));

      QAction* showOnlyBlockAction =
        this->Menu->addAction(QString("Show Only Block%1").arg(multipleBlocks ? "s" : ""));
      this->connect(showOnlyBlockAction, SIGNAL(triggered()), this, SLOT(showOnlyBlock()));

      QAction* showAllBlocksAction = this->Menu->addAction("Show All Blocks");
      this->connect(showAllBlocksAction, SIGNAL(triggered()), this, SLOT(showAllBlocks()));

      QAction* unsetVisibilityAction = this->Menu->addAction(
        QString("Unset Block %1").arg(multipleBlocks ? "Visibilities" : "Visibility"));
      this->connect(unsetVisibilityAction, SIGNAL(triggered()), this, SLOT(unsetBlockVisibility()));

      this->Menu->addSeparator();

      QAction* setBlockColorAction =
        this->Menu->addAction(QString("Set Block Color%1").arg(multipleBlocks ? "s" : ""));
      this->connect(setBlockColorAction, SIGNAL(triggered()), this, SLOT(setBlockColor()));

      QAction* unsetBlockColorAction =
        this->Menu->addAction(QString("Unset Block Color%1").arg(multipleBlocks ? "s" : ""));
      this->connect(unsetBlockColorAction, SIGNAL(triggered()), this, SLOT(unsetBlockColor()));

      this->Menu->addSeparator();

      QAction* setBlockOpacityAction = this->Menu->addAction(
        QString("Set Block %1").arg(multipleBlocks ? "Opacities" : "Opacity"));
      this->connect(setBlockOpacityAction, SIGNAL(triggered()), this, SLOT(setBlockOpacity()));

      QAction* unsetBlockOpacityAction = this->Menu->addAction(
        QString("Unset Block %1").arg(multipleBlocks ? "Opacities" : "Opacity"));
      this->connect(unsetBlockOpacityAction, SIGNAL(triggered()), this, SLOT(unsetBlockOpacity()));

      this->Menu->addSeparator();
    }

    QAction* action;
    action = this->Menu->addAction("Hide");
    QObject::connect(action, SIGNAL(triggered()), this, SLOT(hide()));

    QMenu* reprMenu = this->Menu->addMenu("Representation") << pqSetName("Representation");

    // populate the representation types menu.
    QList<QVariant> rTypes =
      pqSMAdaptor::getEnumerationPropertyDomain(repr->getProxy()->GetProperty("Representation"));
    QVariant curRType =
      pqSMAdaptor::getEnumerationProperty(repr->getProxy()->GetProperty("Representation"));
    foreach (QVariant rtype, rTypes)
    {
      QAction* raction = reprMenu->addAction(rtype.toString());
      raction->setCheckable(true);
      raction->setChecked(rtype == curRType);
    }

    QObject::connect(reprMenu, SIGNAL(triggered(QAction*)), this, SLOT(reprTypeChanged(QAction*)));

    this->Menu->addSeparator();

    pqPipelineRepresentation* pipelineRepr = qobject_cast<pqPipelineRepresentation*>(repr);

    if (pipelineRepr)
    {
      QMenu* colorFieldsMenu = this->Menu->addMenu("Color By") << pqSetName("ColorBy");
      this->buildColorFieldsMenu(pipelineRepr, colorFieldsMenu);
    }

    action = this->Menu->addAction("Edit Color");
    new pqEditColorMapReaction(action);

    this->Menu->addSeparator();
  }
  else
  {
    repr = pqActiveObjects::instance().activeRepresentation();
    if (repr)
    {
      vtkPVDataInformation* info = repr->getInputDataInformation();
      vtkPVCompositeDataInformation* compositeInfo = info->GetCompositeDataInformation();
      if (compositeInfo && compositeInfo->GetDataIsComposite())
      {
        QAction* showAllBlocksAction = this->Menu->addAction("Show All Blocks");
        this->connect(showAllBlocksAction, SIGNAL(triggered()), this, SLOT(showAllBlocks()));
      }
    }
  }

  // when nothing was picked we show the "link camera" menu.
  this->Menu->addAction("Link Camera...", view, SLOT(linkToOtherView()));
}

//-----------------------------------------------------------------------------
void pqPipelineContextMenuBehavior::buildColorFieldsMenu(
  pqPipelineRepresentation* pipelineRepr, QMenu* menu)
{
  QObject::connect(menu, SIGNAL(triggered(QAction*)), this, SLOT(colorMenuTriggered(QAction*)),
    Qt::QueuedConnection);

  QIcon cellDataIcon(":/pqWidgets/Icons/pqCellData16.png");
  QIcon pointDataIcon(":/pqWidgets/Icons/pqPointData16.png");
  QIcon solidColorIcon(":/pqWidgets/Icons/pqSolidColor16.png");

  menu->addAction(solidColorIcon, "Solid Color")->setData(convert(QPair<int, QString>()));
  vtkSMProperty* prop = pipelineRepr->getProxy()->GetProperty("ColorArrayName");
  vtkSMArrayListDomain* domain =
    prop ? vtkSMArrayListDomain::SafeDownCast(prop->FindDomain("vtkSMArrayListDomain")) : NULL;
  if (!domain)
  {
    return;
  }

  // We are only showing array names here without worrying about components since that
  // keeps the menu simple and code even simpler :).
  for (unsigned int cc = 0, max = domain->GetNumberOfStrings(); cc < max; cc++)
  {
    int association = domain->GetFieldAssociation(cc);
    int icon_association = domain->GetDomainAssociation(cc);
    QString name = domain->GetString(cc);

    QIcon& icon = (icon_association == vtkDataObject::CELL) ? cellDataIcon : pointDataIcon;

    QVariant data = convert(QPair<int, QString>(association, name));
    menu->addAction(icon, name)->setData(data);
  }
}

//-----------------------------------------------------------------------------
void pqPipelineContextMenuBehavior::colorMenuTriggered(QAction* action)
{
  QPair<int, QString> array = convert(action->data());
  if (this->PickedRepresentation)
  {
    BEGIN_UNDO_SET("Change coloring");
    vtkSMViewProxy* view = pqActiveObjects::instance().activeView()->getViewProxy();
    vtkSMProxy* reprProxy = this->PickedRepresentation->getProxy();

    vtkSMProxy* oldLutProxy = vtkSMPropertyHelper(reprProxy, "LookupTable", true).GetAsProxy();

    vtkSMPVRepresentationProxy::SetScalarColoring(
      reprProxy, array.second.toLocal8Bit().data(), array.first);

    vtkNew<vtkSMTransferFunctionManager> tmgr;

    // Hide unused scalar bars, if applicable.
    vtkPVGeneralSettings* gsettings = vtkPVGeneralSettings::GetInstance();
    switch (gsettings->GetScalarBarMode())
    {
      case vtkPVGeneralSettings::AUTOMATICALLY_HIDE_SCALAR_BARS:
      case vtkPVGeneralSettings::AUTOMATICALLY_SHOW_AND_HIDE_SCALAR_BARS:
        tmgr->HideScalarBarIfNotNeeded(oldLutProxy, view);
        break;
    }

    if (!array.second.isEmpty())
    {
      // we could now respect some application setting to determine if the LUT is
      // to be reset.
      vtkSMPVRepresentationProxy::RescaleTransferFunctionToDataRange(reprProxy, true);

      /// BUG #0011858. Users often do silly things!
      bool reprVisibility =
        vtkSMPropertyHelper(reprProxy, "Visibility", /*quiet*/ true).GetAsInt() == 1;

      // now show used scalar bars if applicable.
      if (reprVisibility &&
        gsettings->GetScalarBarMode() ==
          vtkPVGeneralSettings::AUTOMATICALLY_SHOW_AND_HIDE_SCALAR_BARS)
      {
        vtkSMPVRepresentationProxy::SetScalarBarVisibility(reprProxy, view, true);
      }
    }

    this->PickedRepresentation->renderViewEventually();
    END_UNDO_SET();
  }
}

//-----------------------------------------------------------------------------
void pqPipelineContextMenuBehavior::reprTypeChanged(QAction* action)
{
  pqDataRepresentation* repr = this->PickedRepresentation;
  if (repr)
  {
    BEGIN_UNDO_SET("Representation Type Changed");
    pqSMAdaptor::setEnumerationProperty(
      repr->getProxy()->GetProperty("Representation"), action->text());
    repr->getProxy()->UpdateVTKObjects();
    repr->renderViewEventually();
    END_UNDO_SET();
  }
}

//-----------------------------------------------------------------------------
void pqPipelineContextMenuBehavior::hide()
{
  pqDataRepresentation* repr = this->PickedRepresentation;
  if (repr)
  {
    BEGIN_UNDO_SET("Visibility Changed");
    repr->setVisible(false);
    repr->renderViewEventually();
    END_UNDO_SET();
  }
}

namespace
{
void readVisibilityMap(vtkSMIntVectorProperty* ivp, QMap<unsigned int, int>& visibilities)
{
  for (unsigned i = 0; i + 1 < ivp->GetNumberOfElements(); i += 2)
  {
    visibilities[ivp->GetElement(i)] = ivp->GetElement(i + 1);
  }
}

void setVisibilitiesFromMap(
  vtkSMIntVectorProperty* ivp, QMap<unsigned int, int>& visibilities, vtkSMProxy* proxy)
{
  std::vector<int> vector;

  for (QMap<unsigned int, int>::const_iterator i = visibilities.begin(); i != visibilities.end();
       i++)
  {
    vector.push_back(static_cast<int>(i.key()));
    vector.push_back(static_cast<int>(i.value()));
  }
  BEGIN_UNDO_SET("Change Block Visibilities");
  if (!vector.empty())
  {
    // if property changes, ModifiedEvent will be fired and
    // this->UpdateUITimer will be started.
    ivp->SetElements(&vector[0], static_cast<unsigned int>(vector.size()));
  }
  proxy->UpdateVTKObjects();
  END_UNDO_SET();
}
}

//-----------------------------------------------------------------------------
void pqPipelineContextMenuBehavior::hideBlock()
{
  QAction* action = qobject_cast<QAction*>(sender());
  if (!action)
  {
    return;
  }

  vtkSMProxy* proxy = this->PickedRepresentation->getProxy();
  vtkSMProperty* property = proxy->GetProperty("BlockVisibility");
  if (property)
  {
    vtkSMIntVectorProperty* ivp = vtkSMIntVectorProperty::SafeDownCast(property);
    QMap<unsigned int, int> visibilities;
    readVisibilityMap(ivp, visibilities);
    for (int i = 0; i < this->PickedBlocks.size(); ++i)
    {
      visibilities[this->PickedBlocks[i]] = 0;
    }
    setVisibilitiesFromMap(ivp, visibilities, proxy);
  }
  this->PickedRepresentation->renderViewEventually();
}

//-----------------------------------------------------------------------------
void pqPipelineContextMenuBehavior::showOnlyBlock()
{
  QAction* action = qobject_cast<QAction*>(sender());
  if (!action)
  {
    return;
  }

  vtkSMProxy* proxy = this->PickedRepresentation->getProxy();
  vtkSMProperty* property = proxy->GetProperty("BlockVisibility");
  if (property)
  {
    vtkSMIntVectorProperty* ivp = vtkSMIntVectorProperty::SafeDownCast(property);
    QMap<unsigned int, int> visibilities;
    visibilities[0] = 0;
    for (int i = 0; i < this->PickedBlocks.size(); ++i)
    {
      visibilities[this->PickedBlocks[i]] = 1;
    }
    setVisibilitiesFromMap(ivp, visibilities, proxy);
  }
  this->PickedRepresentation->renderViewEventually();
}

//-----------------------------------------------------------------------------
void pqPipelineContextMenuBehavior::showAllBlocks()
{
  pqRepresentation* repr = this->PickedRepresentation;
  if (!repr)
  {
    repr = pqActiveObjects::instance().activeRepresentation();
    if (!repr)
    {
      return;
    }
  }
  vtkSMProxy* proxy = repr->getProxy();
  if (!proxy)
  {
    return;
  }
  vtkSMProperty* property = proxy->GetProperty("BlockVisibility");
  if (property)
  {
    vtkSMIntVectorProperty* ivp = vtkSMIntVectorProperty::SafeDownCast(property);
    QMap<unsigned int, int> visibilities;
    visibilities[0] = 1;
    setVisibilitiesFromMap(ivp, visibilities, proxy);
  }
  repr->renderViewEventually();
}

//-----------------------------------------------------------------------------
void pqPipelineContextMenuBehavior::unsetBlockVisibility()
{
  QAction* action = qobject_cast<QAction*>(sender());
  if (!action)
  {
    return;
  }

  vtkSMProxy* proxy = this->PickedRepresentation->getProxy();
  vtkSMProperty* property = proxy->GetProperty("BlockVisibility");
  if (property)
  {
    vtkSMIntVectorProperty* ivp = vtkSMIntVectorProperty::SafeDownCast(property);
    QMap<unsigned int, int> visibilities;
    readVisibilityMap(ivp, visibilities);
    for (int i = 0; i < this->PickedBlocks.size(); ++i)
    {
      visibilities.remove(this->PickedBlocks[i]);
    }
    setVisibilitiesFromMap(ivp, visibilities, proxy);
  }
  this->PickedRepresentation->renderViewEventually();
}

//-----------------------------------------------------------------------------
void pqPipelineContextMenuBehavior::setBlockColor()
{
  QAction* action = qobject_cast<QAction*>(sender());
  if (!action)
  {
    return;
  }

  QColor qcolor = QColorDialog::getColor(QColor(), pqCoreUtilities::mainWidget(),
    "Choose Block Color", QColorDialog::DontUseNativeDialog);

  vtkSMProxy* proxy = this->PickedRepresentation->getProxy();
  vtkSMProperty* property = proxy->GetProperty("BlockColor");
  if (property)
  {
    BEGIN_UNDO_SET("Change Block Colors");
    vtkSMDoubleMapProperty* dmp = vtkSMDoubleMapProperty::SafeDownCast(property);
    for (int i = 0; i < this->PickedBlocks.size(); ++i)
    {
      double color[] = { qcolor.redF(), qcolor.greenF(), qcolor.blueF() };
      dmp->SetElements(this->PickedBlocks[i], color);
    }
    proxy->UpdateVTKObjects();
    END_UNDO_SET();
  }
  this->PickedRepresentation->renderViewEventually();
}

//-----------------------------------------------------------------------------
void pqPipelineContextMenuBehavior::unsetBlockColor()
{
  QAction* action = qobject_cast<QAction*>(sender());
  if (!action)
  {
    return;
  }

  vtkSMProxy* proxy = this->PickedRepresentation->getProxy();
  vtkSMProperty* property = proxy->GetProperty("BlockColor");
  if (property)
  {
    BEGIN_UNDO_SET("Change Block Colors");
    vtkSMDoubleMapProperty* dmp = vtkSMDoubleMapProperty::SafeDownCast(property);
    QMap<unsigned int, QColor> blockColors;
    vtkSmartPointer<vtkSMDoubleMapPropertyIterator> iter;
    iter.TakeReference(dmp->NewIterator());
    for (iter->Begin(); !iter->IsAtEnd(); iter->Next())
    {
      QColor color = QColor::fromRgbF(
        iter->GetElementComponent(0), iter->GetElementComponent(1), iter->GetElementComponent(2));
      blockColors.insert(iter->GetKey(), color);
    }
    for (int i = 0; i < this->PickedBlocks.size(); ++i)
    {
      blockColors.remove(this->PickedBlocks[i]);
    }

    dmp->ClearElements();
    QMap<unsigned int, QColor>::const_iterator iter2;
    for (iter2 = blockColors.begin(); iter2 != blockColors.end(); iter2++)
    {
      QColor qcolor = iter2.value();
      double color[] = { qcolor.redF(), qcolor.greenF(), qcolor.blueF() };
      dmp->SetElements(iter2.key(), color);
    }
    proxy->UpdateVTKObjects();
    END_UNDO_SET();
  }
  this->PickedRepresentation->renderViewEventually();
}

//-----------------------------------------------------------------------------
void pqPipelineContextMenuBehavior::setBlockOpacity()
{
  QAction* action = qobject_cast<QAction*>(sender());
  if (!action)
  {
    return;
  }

  vtkSMProxy* proxy = this->PickedRepresentation->getProxy();
  vtkSMProperty* property = proxy->GetProperty("BlockOpacity");
  if (property)
  {
    vtkSMDoubleMapProperty* dmp = vtkSMDoubleMapProperty::SafeDownCast(property);
    // Hope this works?
    double current_opacity = 1;
    if (dmp->HasElement(this->PickedBlocks[0]))
    {
      current_opacity = dmp->GetElement(this->PickedBlocks[0]);
    }

    pqDoubleRangeDialog dialog("Opacity:", 0.0, 1.0, pqCoreUtilities::mainWidget());
    dialog.setValue(current_opacity);
    bool ok = dialog.exec();
    if (!ok)
    {
      return;
    }
    BEGIN_UNDO_SET("Change Block Opacities");

    for (int i = 0; i < this->PickedBlocks.size(); ++i)
    {
      dmp->SetElement(this->PickedBlocks[i], dialog.value());
    }
    proxy->UpdateVTKObjects();
    END_UNDO_SET();
  }
  this->PickedRepresentation->renderViewEventually();
}

//-----------------------------------------------------------------------------
void pqPipelineContextMenuBehavior::unsetBlockOpacity()
{
  QAction* action = qobject_cast<QAction*>(sender());
  if (!action)
  {
    return;
  }

  vtkSMProxy* proxy = this->PickedRepresentation->getProxy();
  vtkSMProperty* property = proxy->GetProperty("BlockOpacity");
  if (property)
  {
    BEGIN_UNDO_SET("Change Block Opacities");
    vtkSMDoubleMapProperty* dmp = vtkSMDoubleMapProperty::SafeDownCast(property);

    for (int i = 0; i < this->PickedBlocks.size(); ++i)
    {
      dmp->RemoveElement(this->PickedBlocks[i]);
    }
    proxy->UpdateVTKObjects();
    END_UNDO_SET();
  }
  this->PickedRepresentation->renderViewEventually();
}

namespace
{
const char* findBlockName(
  int flatIndexTarget, int& flatIndexCurrent, vtkPVCompositeDataInformation* currentInfo)
{
  // An interior block shouldn't be selected, only blocks with geometry can be
  if (flatIndexCurrent == flatIndexTarget)
  {
    return nullptr;
  }
  for (unsigned int i = 0; i < currentInfo->GetNumberOfChildren(); i++)
  {
    ++flatIndexCurrent;
    if (flatIndexCurrent == flatIndexTarget)
    {
      return currentInfo->GetName(i);
    }
    else if (flatIndexCurrent > flatIndexTarget)
    {
      return nullptr;
    }
    vtkPVDataInformation* childInfo = currentInfo->GetDataInformation(i);
    if (childInfo)
    {
      vtkPVCompositeDataInformation* compositeChildInfo = childInfo->GetCompositeDataInformation();

      // recurse down through child blocks only if the child block
      // is composite and is not a multi-piece data set
      if (compositeChildInfo->GetDataIsComposite() && !compositeChildInfo->GetDataIsMultiPiece())
      {
        const char* result = findBlockName(flatIndexTarget, flatIndexCurrent, compositeChildInfo);
        if (result)
        {
          return result;
        }
      }
      else if (compositeChildInfo && compositeChildInfo->GetDataIsMultiPiece())
      {
        flatIndexCurrent += compositeChildInfo->GetNumberOfChildren();
      }
    }
  }
  return nullptr;
}
}

//-----------------------------------------------------------------------------
QString pqPipelineContextMenuBehavior::lookupBlockName(unsigned int flatIndex) const
{
  vtkPVDataInformation* info = this->PickedRepresentation->getRepresentedDataInformation();
  if (!info)
  {
    return QString();
  }
  vtkPVCompositeDataInformation* compositeInfo = info->GetCompositeDataInformation();

  int myIdx = 0;
  const char* name = findBlockName(flatIndex, myIdx, compositeInfo);
  if (name)
  {
    return QString(name);
  }
  else
  {
    return QString();
  }
}
