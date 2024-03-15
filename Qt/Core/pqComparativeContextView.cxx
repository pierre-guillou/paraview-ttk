// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-FileCopyrightText: Copyright (c) Sandia Corporation
// SPDX-License-Identifier: BSD-3-Clause
#include "pqComparativeContextView.h"

#include "pqQVTKWidget.h"
#include "pqServer.h"
#include "pqUndoStack.h"
#include "vtkCollection.h"
#include "vtkContextView.h"
#include "vtkEventQtSlotConnect.h"
#include "vtkPVServerInformation.h"
#include "vtkSMComparativeViewProxy.h"
#include "vtkSMContextViewProxy.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSmartPointer.h"
#include "vtkWeakPointer.h"

#include <QGridLayout>
#include <QMap>
#include <QPointer>
#include <QSet>
#include <QWidget>

class pqComparativeContextView::pqInternal
{
public:
  QMap<vtkSMViewProxy*, QPointer<pqQVTKWidget>> RenderWidgets;
  vtkSmartPointer<vtkEventQtSlotConnect> VTKConnect;

  pqInternal() { this->VTKConnect = vtkSmartPointer<vtkEventQtSlotConnect>::New(); }
};

namespace
{
/// This helps us monitor QResizeEvent after it has been processed (unlike a
/// generic event filter).
class pqComparativeWidget : public QWidget
{
public:
  vtkWeakPointer<vtkSMProxy> ViewProxy;
  void resizeEvent(QResizeEvent* evt) override
  {
    this->QWidget::resizeEvent(evt);

    QSize asize = this->size() * this->devicePixelRatio();
    int view_size[2];
    view_size[0] = asize.width();
    view_size[1] = asize.height();

    BEGIN_UNDO_EXCLUDE();
    vtkSMPropertyHelper(this->ViewProxy, "ViewSize").Set(view_size, 2);
    this->ViewProxy->UpdateProperty("ViewSize");
    END_UNDO_EXCLUDE();
  }
};
}

//-----------------------------------------------------------------------------
pqComparativeContextView::pqComparativeContextView(const QString& type, const QString& group,
  const QString& name, vtkSMComparativeViewProxy* view, pqServer* server, QObject* parentObject)
  : Superclass(type, group, name, view, server, parentObject)
{
  this->Internal = new pqInternal();
  pqComparativeWidget* wdg = new pqComparativeWidget();
  wdg->ViewProxy = view;
  this->Widget = wdg;
  this->getConnector()->Connect(view, vtkCommand::ConfigureEvent, this, SLOT(updateViewWidgets()));
}

//-----------------------------------------------------------------------------
pqComparativeContextView::~pqComparativeContextView()
{
  Q_FOREACH (pqQVTKWidget* wdg, this->Internal->RenderWidgets.values())
  {
    delete wdg;
  }
  delete this->Internal;
  delete this->Widget;
}

//-----------------------------------------------------------------------------
vtkContextView* pqComparativeContextView::getVTKContextView() const
{
  return vtkSMContextViewProxy::SafeDownCast(this->getViewProxy())->GetContextView();
}

//-----------------------------------------------------------------------------
vtkSMContextViewProxy* pqComparativeContextView::getContextViewProxy() const
{
  return vtkSMContextViewProxy::SafeDownCast(this->getViewProxy());
}

//-----------------------------------------------------------------------------
QWidget* pqComparativeContextView::createWidget()
{
  // widget is already created. Return that.
  this->updateViewWidgets();
  return this->Widget;
}

//-----------------------------------------------------------------------------
vtkSMComparativeViewProxy* pqComparativeContextView::getComparativeViewProxy() const
{
  return vtkSMComparativeViewProxy::SafeDownCast(this->getProxy());
}

//-----------------------------------------------------------------------------
vtkSMViewProxy* pqComparativeContextView::getViewProxy() const
{
  return this->getComparativeViewProxy()->GetRootView();
}

//-----------------------------------------------------------------------------
void pqComparativeContextView::updateViewWidgets()
{
  // This logic is adapted from pqComparativeRenderView, the two should be
  // consolidated/refactored to have a common base class.
  // Create QVTKWidgets for new view modules and destroy old ones.
  vtkCollection* currentViews = vtkCollection::New();

  vtkSMComparativeViewProxy* compView = vtkSMComparativeViewProxy::SafeDownCast(this->getProxy());
  compView->GetViews(currentViews);

  QSet<vtkSMViewProxy*> currentViewsSet;

  currentViews->InitTraversal();
  vtkSMViewProxy* temp = vtkSMViewProxy::SafeDownCast(currentViews->GetNextItemAsObject());
  for (; temp != nullptr; temp = vtkSMViewProxy::SafeDownCast(currentViews->GetNextItemAsObject()))
  {
    currentViewsSet.insert(temp);
  }

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
  QSet<vtkSMViewProxy*> oldViews =
    QSet<vtkSMViewProxy*>::fromList(this->Internal->RenderWidgets.keys());
#else
  auto const& render_keys = this->Internal->RenderWidgets.keys();
  QSet<vtkSMViewProxy*> oldViews = QSet<vtkSMViewProxy*>(render_keys.begin(), render_keys.end());
#endif

  QSet<vtkSMViewProxy*> removed = oldViews - currentViewsSet;
  QSet<vtkSMViewProxy*> added = currentViewsSet - oldViews;

  // Destroy old QVTKWidgets widgets.
  Q_FOREACH (vtkSMViewProxy* key, removed)
  {
    pqQVTKWidget* item = this->Internal->RenderWidgets.take(key);
    delete item;
  }

  // Create QVTKWidgets for new ones.
  Q_FOREACH (vtkSMViewProxy* key, added)
  {
    vtkSMContextViewProxy* cntxtView = vtkSMContextViewProxy::SafeDownCast(key);
    cntxtView->UpdateVTKObjects();

    pqQVTKWidget* wdg = new pqQVTKWidget();
    wdg->setRenderWindow(cntxtView->GetContextView()->GetRenderWindow());
    cntxtView->SetupInteractor(wdg->interactor());
    wdg->installEventFilter(this);
    wdg->setContextMenuPolicy(Qt::NoContextMenu);
    this->Internal->RenderWidgets[key] = wdg;
  }

  // Now layout the views.
  int dimensions[2];
  vtkSMPropertyHelper(compView, "Dimensions").Get(dimensions, 2);
  if (vtkSMPropertyHelper(compView, "OverlayAllComparisons").GetAsInt() != 0)
  {
    dimensions[0] = dimensions[1] = 1;
  }

  // destroy the old layout and create a new one.
  QWidget* wdg = this->Widget;
  delete wdg->layout();

  QGridLayout* layout = new QGridLayout(wdg);
  layout->setHorizontalSpacing(vtkSMPropertyHelper(compView, "Spacing").GetAsInt(0));
  layout->setVerticalSpacing(vtkSMPropertyHelper(compView, "Spacing").GetAsInt(1));
  layout->setContentsMargins(0, 0, 0, 0);
  for (int x = 0; x < dimensions[0]; ++x)
  {
    for (int y = 0; y < dimensions[1]; ++y)
    {
      int index = y * dimensions[0] + x;
      vtkSMViewProxy* view = vtkSMViewProxy::SafeDownCast(currentViews->GetItemAsObject(index));
      pqQVTKWidget* vtkwidget = this->Internal->RenderWidgets[view];
      layout->addWidget(vtkwidget, y, x);
    }
  }

  currentViews->Delete();
}
