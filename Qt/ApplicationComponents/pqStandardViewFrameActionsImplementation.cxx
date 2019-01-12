/*=========================================================================

   Program: ParaView
   Module:    pqStandardViewFrameActionsImplementation.cxx

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
#include "pqStandardViewFrameActionsImplementation.h"
#include "ui_pqEmptyView.h"

#include "pqActiveObjects.h"
#include "pqApplicationCore.h"
#include "pqCameraUndoRedoReaction.h"
#include "pqChartSelectionReaction.h"
#include "pqContextView.h"
#include "pqCoreUtilities.h"
#include "pqDataQueryReaction.h"
#include "pqEditCameraReaction.h"
#include "pqInterfaceTracker.h"
#include "pqObjectBuilder.h"
#include "pqRenameProxyReaction.h"
#include "pqRenderView.h"
#include "pqRenderViewSelectionReaction.h"
#include "pqServer.h"
#include "pqSpreadSheetView.h"
#include "pqSpreadSheetViewDecorator.h"
#include "pqToggleInteractionViewMode.h"
#include "pqUndoStack.h"
#include "pqViewFrame.h"

#include "vtkChart.h"
#include "vtkCollection.h"
#include "vtkPVProxyDefinitionIterator.h"
#include "vtkPVXMLElement.h"
#include "vtkSMInteractiveSelectionPipeline.h"
#include "vtkSMProxy.h"
#include "vtkSMProxyDefinitionManager.h"
#include "vtkSMRenderViewProxy.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMTooltipSelectionPipeline.h"
#include "vtkSmartPointer.h"

#include <QKeyEvent>
#include <QMenu>
#include <QPushButton>
#include <QSet>
#include <QShortcut>
#include <QStyle>

//-----------------------------------------------------------------------------
pqStandardViewFrameActionsImplementation::pqStandardViewFrameActionsImplementation(
  QObject* parentObject)
  : QObject(parentObject)
{
  QWidget* mainWindow = pqCoreUtilities::mainWidget();
  this->ShortCutSurfaceCells = new QShortcut(QKeySequence(tr("s")), mainWindow);
  this->ShortCutSurfacePoints = new QShortcut(QKeySequence(tr("d")), mainWindow);
  this->ShortCutFrustumCells = new QShortcut(QKeySequence(tr("f")), mainWindow);
  this->ShortCutFrustumPoints = new QShortcut(QKeySequence(tr("g")), mainWindow);
  this->ShortCutBlocks = new QShortcut(QKeySequence("b"), mainWindow);

  QObject::connect(
    this->ShortCutSurfaceCells, SIGNAL(activated()), this, SLOT(selectSurfaceCellsTriggered()));
  QObject::connect(
    this->ShortCutSurfacePoints, SIGNAL(activated()), this, SLOT(selectSurfacePointsTriggered()));
  QObject::connect(
    this->ShortCutFrustumCells, SIGNAL(activated()), this, SLOT(selectFrustumCellsTriggered()));
  QObject::connect(
    this->ShortCutFrustumPoints, SIGNAL(activated()), this, SLOT(selectFrustumPointsTriggered()));
  QObject::connect(this->ShortCutBlocks, SIGNAL(activated()), this, SLOT(selectBlocksTriggered()));

  this->ShortCutEsc = new QShortcut(QKeySequence(Qt::Key_Escape), mainWindow);
  this->ShortCutEsc->setEnabled(false);
  this->connect(this->ShortCutEsc, SIGNAL(activated()), SLOT(escTriggered()));
}

//-----------------------------------------------------------------------------
pqStandardViewFrameActionsImplementation::~pqStandardViewFrameActionsImplementation()
{
  delete this->ShortCutSurfaceCells;
  delete this->ShortCutSurfacePoints;
  delete this->ShortCutFrustumCells;
  delete this->ShortCutFrustumPoints;
  delete this->ShortCutBlocks;
}

//-----------------------------------------------------------------------------
void pqStandardViewFrameActionsImplementation::frameConnected(pqViewFrame* frame, pqView* view)
{
  Q_ASSERT(frame != NULL);
  if (view == NULL)
  {
    // Setup the UI shown when no view is present in the frame.
    QWidget* empty_frame = new QWidget(frame);
    this->setupEmptyFrame(empty_frame);
    frame->setCentralWidget(empty_frame);
  }
  else
  {
    // add view-type independent actions.
    frame->setTitle(view->getSMName());
    this->addGenericActions(frame, view);
    if (pqContextView* const chart_view = qobject_cast<pqContextView*>(view))
    {
      this->addContextViewActions(frame, chart_view);
    }
    else if (pqRenderView* const render_view = qobject_cast<pqRenderView*>(view))
    {
      this->addRenderViewActions(frame, render_view);
    }
    else if (pqSpreadSheetView* const sp_view = qobject_cast<pqSpreadSheetView*>(view))
    {
      this->addSpreadSheetViewActions(frame, sp_view);
    }
  }
}

//-----------------------------------------------------------------------------
void pqStandardViewFrameActionsImplementation::addContextViewActions(
  pqViewFrame* frame, pqContextView* chart_view)
{
  // Adding special selection controls for chart/context view
  Q_ASSERT(chart_view);
  Q_ASSERT(frame);

  QActionGroup* modeGroup = this->addSelectionModifierActions(frame, chart_view);
  QActionGroup* group = new QActionGroup(frame);

  this->addSeparator(frame, chart_view);

  if (this->isButtonVisible("SelectPolygon", chart_view))
  {
    QAction* chartSelectPolygonAction = frame->addTitleBarAction(
      QIcon(":/pqWidgets/Icons/pqSelectChartPolygon16.png"), "Polygon Selection (d)");
    chartSelectPolygonAction->setObjectName("actionChartSelectPolygon");
    chartSelectPolygonAction->setCheckable(true);
    chartSelectPolygonAction->setData(QVariant(vtkChart::SELECT_POLYGON));
    this->connect(
      chartSelectPolygonAction, SIGNAL(toggled(bool)), SLOT(escapeableActionToggled(bool)));
    group->addAction(chartSelectPolygonAction);
    new pqChartSelectionReaction(chartSelectPolygonAction, chart_view, modeGroup);
  }

  if (this->isButtonVisible("SelectRectangle", chart_view))
  {
    QAction* chartSelectRectangularAction = frame->addTitleBarAction(
      QIcon(":/pqWidgets/Icons/pqSelectChart16.png"), "Rectangle Selection (s)");
    chartSelectRectangularAction->setObjectName("actionChartSelectRectangle");
    chartSelectRectangularAction->setCheckable(true);
    chartSelectRectangularAction->setData(QVariant(vtkChart::SELECT_RECTANGLE));
    this->connect(
      chartSelectRectangularAction, SIGNAL(toggled(bool)), SLOT(escapeableActionToggled(bool)));
    new pqChartSelectionReaction(chartSelectRectangularAction, chart_view, modeGroup);
    group->addAction(chartSelectRectangularAction);
  }

  /// If a QAction is added to an exclusive QActionGroup, then a checked action
  /// cannot be unchecked by clicking on it. We need that to work. Hence, we
  /// manually manage the exclusivity of the action group.
  group->setExclusive(false);
  this->QObject::connect(
    group, SIGNAL(triggered(QAction*)), SLOT(manageGroupExclusivity(QAction*)));
}

//-----------------------------------------------------------------------------
QActionGroup* pqStandardViewFrameActionsImplementation::addSelectionModifierActions(
  pqViewFrame* frame, pqView* view)
{
  Q_ASSERT(view);
  Q_ASSERT(frame);

  QAction* toggleAction = NULL;
  QAction* minusAction = NULL;
  QAction* plusAction = NULL;

  this->addSeparator(frame, view);

  if (this->isButtonVisible("AddSelection", view))
  {
    plusAction = frame->addTitleBarAction(
      QIcon(":/pqWidgets/Icons/pqSelectPlus16.png"), tr("Add selection (Ctrl)"));
    plusAction->setObjectName("actionAddSelection");
    plusAction->setCheckable(true);
    plusAction->setData(QVariant(pqView::PV_SELECTION_ADDITION));
  }

  if (this->isButtonVisible("SubtractSelection", view))
  {
    minusAction = frame->addTitleBarAction(
      QIcon(":/pqWidgets/Icons/pqSelectMinus16.png"), tr("Subtract selection (Shift)"));
    minusAction->setObjectName("actionSubtractSelection");
    minusAction->setCheckable(true);
    minusAction->setData(QVariant(pqView::PV_SELECTION_SUBTRACTION));
  }

  if (this->isButtonVisible("ToggleSelection", view))
  {
    toggleAction = frame->addTitleBarAction(
      QIcon(":/pqWidgets/Icons/pqSelectToggle16.png"), tr("Toggle selection (Ctrl+Shift)"));
    toggleAction->setObjectName("actionToggleSelection");
    toggleAction->setCheckable(true);
    toggleAction->setData(QVariant(pqView::PV_SELECTION_TOGGLE));
  }

  QActionGroup* modeGroup = new QActionGroup(frame);
  if (plusAction)
  {
    modeGroup->addAction(plusAction);
  }
  if (minusAction)
  {
    modeGroup->addAction(minusAction);
  }
  if (toggleAction)
  {
    modeGroup->addAction(toggleAction);
  }

  /// If a QAction is added to an exclusive QActionGroup, then a checked action
  /// cannot be unchecked by clicking on it. We need that to work. Hence, we
  /// manually manage the exclusivity of the action group.
  modeGroup->setExclusive(false);
  this->QObject::connect(
    modeGroup, SIGNAL(triggered(QAction*)), SLOT(manageGroupExclusivity(QAction*)));

  return modeGroup;
}

//-----------------------------------------------------------------------------
void pqStandardViewFrameActionsImplementation::addSeparator(pqViewFrame* frame, pqView* view)
{
  if (this->isButtonVisible("Separator", view))
  {
    frame->addTitleBarSeparator();
  }
}

//-----------------------------------------------------------------------------
void pqStandardViewFrameActionsImplementation::addGenericActions(pqViewFrame* frame, pqView* view)
{
  Q_ASSERT(frame);
  Q_ASSERT(view);

  /// Add convert-to menu.
  frame->contextMenu()->addSeparator();
  QAction* renameAction = frame->contextMenu()->addAction("Rename");
  new pqRenameProxyReaction(renameAction, view);

  QMenu* convertMenu = frame->contextMenu()->addMenu("Convert To ...");
  QObject::connect(convertMenu, SIGNAL(aboutToShow()), this, SLOT(aboutToShowConvertMenu()));

  if (view->supportsUndo())
  {
    // Setup undo/redo connections if the view module
    // supports interaction undo.
    if (this->isButtonVisible("BackButton", view))
    {
      QAction* backAction =
        frame->addTitleBarAction(QIcon(":/pqWidgets/Icons/pqUndoCamera24.png"), "Camera Undo");
      backAction->setObjectName("actionBackButton");
      new pqCameraUndoRedoReaction(backAction, true, view);
    }

    if (this->isButtonVisible("ForwardButton", view))
    {
      QAction* forwardAction =
        frame->addTitleBarAction(QIcon(":/pqWidgets/Icons/pqRedoCamera24.png"), "Camera Redo");
      forwardAction->setObjectName("actionForwardButton");
      new pqCameraUndoRedoReaction(forwardAction, false, view);
    }
  }
}

//-----------------------------------------------------------------------------
void pqStandardViewFrameActionsImplementation::addRenderViewActions(
  pqViewFrame* frame, pqRenderView* renderView)
{
  Q_ASSERT(renderView);
  Q_ASSERT(frame);

  this->addSeparator(frame, renderView);

  if (this->isButtonVisible("ToggleInteractionMode", renderView))
  {
    QAction* toggleInteractionModeAction = frame->addTitleBarAction(
      QIcon(":/pqWidgets/Icons/pqInteractionMode3D16.png"), "Change Interaction Mode");
    toggleInteractionModeAction->setObjectName("actionToggleInteractionMode");
    new pqToggleInteractionViewMode(toggleInteractionModeAction, renderView);
  }

  if (this->isButtonVisible("AdjustCamera", renderView))
  {
    QAction* adjustCameraAction =
      frame->addTitleBarAction(QIcon(":/pqWidgets/Icons/pqEditCamera16.png"), "Adjust Camera");
    adjustCameraAction->setObjectName("actionAdjustCamera");
    new pqEditCameraReaction(adjustCameraAction, renderView);
  }

  QActionGroup* modeGroup = this->addSelectionModifierActions(frame, renderView);

  this->addSeparator(frame, renderView);

  if (this->isButtonVisible("SelectSurfaceCells", renderView))
  {
    QAction* selectSurfaceCellsAction = frame->addTitleBarAction(
      QIcon(":/pqWidgets/Icons/pqSurfaceSelectionCell24.png"), "Select Cells On (s)");
    selectSurfaceCellsAction->setObjectName("actionSelectSurfaceCells");
    selectSurfaceCellsAction->setCheckable(true);
    new pqRenderViewSelectionReaction(selectSurfaceCellsAction, renderView,
      pqRenderViewSelectionReaction::SELECT_SURFACE_CELLS, modeGroup);
    this->connect(
      selectSurfaceCellsAction, SIGNAL(toggled(bool)), SLOT(escapeableActionToggled(bool)));
  }

  if (this->isButtonVisible("SelectSurfacePoints", renderView))
  {
    QAction* selectSurfacePointsAction = frame->addTitleBarAction(
      QIcon(":/pqWidgets/Icons/pqSurfaceSelectionPoint24.png"), "Select Points On (d)");
    selectSurfacePointsAction->setObjectName("actionSelectSurfacePoints");
    selectSurfacePointsAction->setCheckable(true);
    new pqRenderViewSelectionReaction(selectSurfacePointsAction, renderView,
      pqRenderViewSelectionReaction::SELECT_SURFACE_POINTS, modeGroup);
    this->connect(
      selectSurfacePointsAction, SIGNAL(toggled(bool)), SLOT(escapeableActionToggled(bool)));
  }

  if (this->isButtonVisible("SelectFrustumCells", renderView))
  {
    QAction* selectFrustumCellsAction = frame->addTitleBarAction(
      QIcon(":/pqWidgets/Icons/pqFrustumSelectionCell24.png"), "Select Cells Through (f)");
    selectFrustumCellsAction->setObjectName("actionSelectFrustumCells");
    selectFrustumCellsAction->setCheckable(true);
    new pqRenderViewSelectionReaction(selectFrustumCellsAction, renderView,
      pqRenderViewSelectionReaction::SELECT_FRUSTUM_CELLS, modeGroup);
    this->connect(
      selectFrustumCellsAction, SIGNAL(toggled(bool)), SLOT(escapeableActionToggled(bool)));
  }

  if (this->isButtonVisible("SelectFrustumPoints", renderView))
  {
    QAction* selectFrustumPointsAction = frame->addTitleBarAction(
      QIcon(":/pqWidgets/Icons/pqFrustumSelectionPoint24.png"), "Select Points Through (g)");
    selectFrustumPointsAction->setObjectName("actionSelectFrustumPoints");
    selectFrustumPointsAction->setCheckable(true);
    new pqRenderViewSelectionReaction(selectFrustumPointsAction, renderView,
      pqRenderViewSelectionReaction::SELECT_FRUSTUM_POINTS, modeGroup);
    this->connect(
      selectFrustumPointsAction, SIGNAL(toggled(bool)), SLOT(escapeableActionToggled(bool)));
  }

  if (this->isButtonVisible("SelectPolygonSelectionCells", renderView))
  {
    QAction* selectionPolygonCellsAction = frame->addTitleBarAction(
      QIcon(":/pqWidgets/Icons/pqPolygonSelectSurfaceCell24.png"), "Select Cells With Polygon");
    selectionPolygonCellsAction->setObjectName("actionPolygonSelectionCells");
    selectionPolygonCellsAction->setCheckable(true);
    new pqRenderViewSelectionReaction(selectionPolygonCellsAction, renderView,
      pqRenderViewSelectionReaction::SELECT_SURFACE_CELLS_POLYGON, modeGroup);
    this->connect(
      selectionPolygonCellsAction, SIGNAL(toggled(bool)), SLOT(escapeableActionToggled(bool)));
  }

  if (this->isButtonVisible("SelectPolygonSelectionPoints", renderView))
  {
    QAction* selectionPolygonPointsAction = frame->addTitleBarAction(
      QIcon(":/pqWidgets/Icons/pqPolygonSelectSurfacePoint24.png"), "Select Points With Polygon");
    selectionPolygonPointsAction->setObjectName("actionPolygonSelectionPoints");
    selectionPolygonPointsAction->setCheckable(true);
    new pqRenderViewSelectionReaction(selectionPolygonPointsAction, renderView,
      pqRenderViewSelectionReaction::SELECT_SURFACE_POINTS_POLYGON, modeGroup);
    this->connect(
      selectionPolygonPointsAction, SIGNAL(toggled(bool)), SLOT(escapeableActionToggled(bool)));
  }

  if (this->isButtonVisible("SelectBlock", renderView))
  {
    QAction* selectBlockAction =
      frame->addTitleBarAction(QIcon(":/pqWidgets/Icons/pqSelectBlock24.png"), "Select Block (b)");
    selectBlockAction->setObjectName("actionSelectBlock");
    selectBlockAction->setCheckable(true);
    new pqRenderViewSelectionReaction(
      selectBlockAction, renderView, pqRenderViewSelectionReaction::SELECT_BLOCKS, modeGroup);
    this->connect(selectBlockAction, SIGNAL(toggled(bool)), SLOT(escapeableActionToggled(bool)));
  }

  if (this->isButtonVisible("InteractiveSelectSurfaceCells", renderView))
  {
    QAction* interactiveSelectSurfaceCellsAction =
      frame->addTitleBarAction(QIcon(":/pqWidgets/Icons/pqSurfaceSelectionCellInteractive.png"),
        "Interactive Select Cells On");
    interactiveSelectSurfaceCellsAction->setObjectName("actionInteractiveSelectSurfaceCells");
    interactiveSelectSurfaceCellsAction->setCheckable(true);
    new pqRenderViewSelectionReaction(interactiveSelectSurfaceCellsAction, renderView,
      pqRenderViewSelectionReaction::SELECT_SURFACE_CELLS_INTERACTIVELY, modeGroup);
    this->connect(interactiveSelectSurfaceCellsAction, SIGNAL(toggled(bool)),
      SLOT(escapeableActionToggled(bool)));
    this->connect(interactiveSelectSurfaceCellsAction, SIGNAL(toggled(bool)),
      SLOT(interactiveSelectionToggled(bool)));
  }

  if (this->isButtonVisible("InteractiveSelectSurfacePoints", renderView))
  {
    QAction* interactiveSelectSurfacePointsAction =
      frame->addTitleBarAction(QIcon(":/pqWidgets/Icons/pqSurfaceSelectionPointInteractive.png"),
        "Interactive Select Points On");
    interactiveSelectSurfacePointsAction->setObjectName("actionInteractiveSelectSurfacePoints");
    interactiveSelectSurfacePointsAction->setCheckable(true);
    new pqRenderViewSelectionReaction(interactiveSelectSurfacePointsAction, renderView,
      pqRenderViewSelectionReaction::SELECT_SURFACE_POINTS_INTERACTIVELY, modeGroup);
    this->connect(interactiveSelectSurfacePointsAction, SIGNAL(toggled(bool)),
      SLOT(escapeableActionToggled(bool)));
    this->connect(interactiveSelectSurfacePointsAction, SIGNAL(toggled(bool)),
      SLOT(interactiveSelectionToggled(bool)));
  }

  if (this->isButtonVisible("HoveringSurfacePoints", renderView))
  {
    QAction* hoveringSurfacePointsAction = frame->addTitleBarAction(
      QIcon(":/pqWidgets/Icons/pqSurfaceHoveringPoint.png"), "Hover Points On");
    hoveringSurfacePointsAction->setObjectName("actionHoveringSurfacePoints");
    hoveringSurfacePointsAction->setCheckable(true);
    new pqRenderViewSelectionReaction(hoveringSurfacePointsAction, renderView,
      pqRenderViewSelectionReaction::SELECT_SURFACE_POINTS_TOOLTIP);
    this->connect(
      hoveringSurfacePointsAction, SIGNAL(toggled(bool)), SLOT(escapeableActionToggled(bool)));
    this->connect(
      hoveringSurfacePointsAction, SIGNAL(toggled(bool)), SLOT(interactiveSelectionToggled(bool)));
  }

  if (this->isButtonVisible("HoveringSurfaceCells", renderView))
  {
    QAction* hoveringSurfaceCellsAction = frame->addTitleBarAction(
      QIcon(":/pqWidgets/Icons/pqSurfaceHoveringCell.png"), "Hover Cells On");
    hoveringSurfaceCellsAction->setObjectName("actionHoveringSurfaceCells");
    hoveringSurfaceCellsAction->setCheckable(true);
    new pqRenderViewSelectionReaction(hoveringSurfaceCellsAction, renderView,
      pqRenderViewSelectionReaction::SELECT_SURFACE_CELLS_TOOLTIP);
    this->connect(
      hoveringSurfaceCellsAction, SIGNAL(toggled(bool)), SLOT(escapeableActionToggled(bool)));
    this->connect(
      hoveringSurfaceCellsAction, SIGNAL(toggled(bool)), SLOT(interactiveSelectionToggled(bool)));
  }

  if (this->isButtonVisible("ClearSelection", renderView))
  {
    QStyle* style = qApp->style();
    QAction* clearAction = frame->addTitleBarAction(
      style->standardIcon(QStyle::SP_DialogDiscardButton), "Clear selection");
    clearAction->setObjectName("actionClearSelection");
    new pqRenderViewSelectionReaction(
      clearAction, renderView, pqRenderViewSelectionReaction::CLEAR_SELECTION);
  }
}

//-----------------------------------------------------------------------------
void pqStandardViewFrameActionsImplementation::addSpreadSheetViewActions(
  pqViewFrame* frame, pqSpreadSheetView* spreadSheet)
{
  Q_ASSERT(frame);
  Q_ASSERT(spreadSheet);
  Q_UNUSED(frame);
  new pqSpreadSheetViewDecorator(spreadSheet);
}

//-----------------------------------------------------------------------------
bool pqStandardViewFrameActionsImplementation::isButtonVisible(
  const std::string& buttonName, pqView* view)
{
  vtkPVXMLElement* hints = view->getHints();
  if (!hints)
  {
    // Default to true
    return true;
  }

  bool isVisible = true;
  vtkPVXMLElement* svfa = hints->FindNestedElementByName("StandardViewFrameActions");
  if (svfa)
  {
    // See if we should disable all view frame actions
    std::string defaultActions(svfa->GetAttributeOrEmpty("default_actions"));
    vtkPVXMLElement* buttonElement = svfa->FindNestedElementByName(buttonName.c_str());
    std::string visibility;
    if (buttonElement)
    {
      visibility = std::string(buttonElement->GetAttributeOrEmpty("visibility"));
    }

    isVisible = visibility != "never";
    if (defaultActions == "none")
    {
      // Turn all actions off *unless* the button has been
      // explicitly enabled by listing them as child elements
      isVisible = isVisible && buttonElement != NULL;
    }
  }

  return isVisible;
}

//-----------------------------------------------------------------------------
// Comparator for strings with a twist. It tries to put strings with "Render
// View" at the top of the sorted list.
bool pqStandardViewFrameActionsImplementation::ViewTypeComparator(
  const pqStandardViewFrameActionsImplementation::ViewType& one,
  const pqStandardViewFrameActionsImplementation::ViewType& two)
{
  bool inone = one.Label.contains("Render View", Qt::CaseInsensitive);
  bool intwo = two.Label.contains("Render View", Qt::CaseInsensitive);

  if ((inone && intwo) || (!inone && !intwo))
  {
    return one.Label.toLower() < two.Label.toLower();
  }
  Q_ASSERT(inone || intwo);
  // one is less if it has "Render View", else two is less.
  return inone;
}

//-----------------------------------------------------------------------------
QList<pqStandardViewFrameActionsImplementation::ViewType>
pqStandardViewFrameActionsImplementation::availableViewTypes()
{
  // Iterate over all available "views".
  QList<ViewType> views;
  pqServer* server = pqActiveObjects::instance().activeServer();
  if (!server)
  {
    return views;
  }
  vtkSMSessionProxyManager* pxm = server->proxyManager();
  vtkSmartPointer<vtkPVProxyDefinitionIterator> iter;
  iter.TakeReference(pxm->GetProxyDefinitionManager()->NewSingleGroupIterator("views"));
  for (iter->InitTraversal(); !iter->IsDoneWithTraversal(); iter->GoToNextItem())
  {
    vtkSMProxy* prototype = pxm->GetPrototypeProxy("views", iter->GetProxyName());
    if (prototype)
    {
      ViewType info;
      info.Label = prototype->GetXMLLabel();
      info.Name = iter->GetProxyName();
      views.push_back(info);
    }
  }
  qSort(views.begin(), views.end(), pqStandardViewFrameActionsImplementation::ViewTypeComparator);
  return views;
}

//-----------------------------------------------------------------------------
void pqStandardViewFrameActionsImplementation::aboutToShowConvertMenu()
{
  QMenu* menu = qobject_cast<QMenu*>(this->sender());
  if (menu)
  {
    menu->clear();
    QList<ViewType> views = this->availableViewTypes();
    foreach (const ViewType& type, views)
    {
      QAction* view_action = new QAction(type.Label, menu);
      view_action->setProperty("PV_VIEW_TYPE", type.Name);
      view_action->setProperty("PV_VIEW_LABEL", type.Label);
      view_action->setProperty("PV_COMMAND", "Convert To");
      menu->addAction(view_action);
      QObject::connect(
        view_action, SIGNAL(triggered()), this, SLOT(invoked()), Qt::QueuedConnection);
    }
  }
}

//-----------------------------------------------------------------------------
void pqStandardViewFrameActionsImplementation::setupEmptyFrame(QWidget* frame)
{
  Ui::EmptyView ui;
  ui.setupUi(frame);

  QList<ViewType> views = this->availableViewTypes();
  foreach (const ViewType& type, views)
  {
    QPushButton* button = new QPushButton(type.Label, ui.ConvertActionsFrame);
    button->setObjectName(type.Name);
    button->setProperty("PV_VIEW_TYPE", type.Name);
    button->setProperty("PV_VIEW_LABEL", type.Label);
    button->setProperty("PV_COMMAND", "Create");

    QObject::connect(button, SIGNAL(clicked()), this, SLOT(invoked()), Qt::QueuedConnection);
    ui.ConvertActionsFrame->layout()->addWidget(button);
  }
}

//-----------------------------------------------------------------------------
void pqStandardViewFrameActionsImplementation::invoked()
{
  QObject* osender = this->sender();
  if (!osender)
  {
    return;
  }

  // either create a new view, or convert the existing one.
  // This slot is called either from an action in the "Convert To" menu, or from
  // the buttons on an empty frame.
  QString type = osender->property("PV_VIEW_TYPE").toString();
  QString label = osender->property("PV_VIEW_LABEL").toString();
  QString command = osender->property("PV_COMMAND").toString();

  BEGIN_UNDO_SET(QString("%1 %2").arg(command).arg(label));
  ViewType vtype;
  vtype.Label = label;
  vtype.Name = type;
  this->handleCreateView(vtype);
  END_UNDO_SET();
}

//-----------------------------------------------------------------------------
void pqStandardViewFrameActionsImplementation::handleCreateView(
  const pqStandardViewFrameActionsImplementation::ViewType& viewType)
{
  pqObjectBuilder* builder = pqApplicationCore::instance()->getObjectBuilder();

  // destroy active-view, if present (implying convert was called).
  if (pqActiveObjects::instance().activeView())
  {
    builder->destroy(pqActiveObjects::instance().activeView());
  }
  if (viewType.Name != "None")
  {
    builder->createView(viewType.Name, pqActiveObjects::instance().activeServer());
  }
}

//-----------------------------------------------------------------------------
namespace
{
QAction* findActiveAction(const QString& name)
{
  pqView* activeView = pqActiveObjects::instance().activeView();
  if (activeView && activeView->widget() && activeView->widget()->parentWidget() &&
    activeView->widget()->parentWidget()->parentWidget())
  {
    return activeView->widget()->parentWidget()->parentWidget()->findChild<QAction*>(name);
  }
  return NULL;
}

void triggerAction(const QString& name)
{
  QAction* atcn = findActiveAction(name);
  if (atcn)
  {
    atcn->trigger();
  }
}
}

//-----------------------------------------------------------------------------
void pqStandardViewFrameActionsImplementation::selectSurfaceCellsTriggered()
{
  pqView* activeView = pqActiveObjects::instance().activeView();
  pqContextView* chartView = qobject_cast<pqContextView*>(activeView);

  if (chartView)
  {
    // if we are in a chart view then trigger the chart selection
    triggerAction("actionChartSelectRectangle");
  }
  else
  {
    // else trigger the render view selection
    triggerAction("actionSelectSurfaceCells");
  }
}

//-----------------------------------------------------------------------------
void pqStandardViewFrameActionsImplementation::selectSurfacePointsTriggered()
{
  pqView* activeView = pqActiveObjects::instance().activeView();
  pqContextView* chartView = qobject_cast<pqContextView*>(activeView);

  if (chartView)
  {
    // if we are in a chart view then trigger the chart selection
    triggerAction("actionChartSelectPolygon");
  }
  else
  {
    // else trigger the render view selection
    triggerAction("actionSelectSurfacePoints");
  }
}

//-----------------------------------------------------------------------------
void pqStandardViewFrameActionsImplementation::selectFrustumCellsTriggered()
{
  triggerAction("actionSelectFrustumCells");
}

//-----------------------------------------------------------------------------
void pqStandardViewFrameActionsImplementation::selectFrustumPointsTriggered()
{
  triggerAction("actionSelectFrustumPoints");
}

//-----------------------------------------------------------------------------
void pqStandardViewFrameActionsImplementation::selectBlocksTriggered()
{
  triggerAction("actionSelectBlock");
}

//-----------------------------------------------------------------------------
void pqStandardViewFrameActionsImplementation::escTriggered()
{
  QAction* actn =
    qobject_cast<QAction*>(this->ShortCutEsc->property("PV_ACTION").value<QObject*>());
  if (actn && actn->isChecked() && actn->isEnabled())
  {
    actn->trigger();
  }
  // this is not necessary for the most part, but just to be on the safe side.
  this->ShortCutEsc->setEnabled(false);
}

//-----------------------------------------------------------------------------
void pqStandardViewFrameActionsImplementation::manageGroupExclusivity(QAction* curAction)
{
  if (!curAction || !curAction->isChecked())
  {
    return;
  }

  QActionGroup* group = qobject_cast<QActionGroup*>(this->sender());
  foreach (QAction* groupAction, group->actions())
  {
    if (groupAction != curAction && groupAction->isChecked())
    {
      groupAction->setChecked(false);
    }
  }
}

//-----------------------------------------------------------------------------
void pqStandardViewFrameActionsImplementation::escapeableActionToggled(bool checked)
{
  // If a "selection mode" begins, we will enable the ShortCutEsc to start monitoring
  // the Esc key to end selection.
  // If "selection mode" ends (due to one reason or another) and the selection that
  // ended was indeed the one which we're monitoring the Esc key for, then we
  // disable the Esc shortcut since it is no longer needed. This disabling
  // ensure that the shortcut doesn't eat away Esc keys which interferes with
  // the Esc key in the Search box, for example.
  QAction* actn = qobject_cast<QAction*>(this->sender());
  if (!actn || !actn->isEnabled() || !actn->isCheckable())
  {
    return;
  }

  if (!checked)
  {
    if (this->ShortCutEsc->property("PV_ACTION").value<QObject*>() == actn)
    {
      this->ShortCutEsc->setEnabled(false);
    }
    return;
  }

  // User has entered into a selection mode. Let's add a shortcut to "catch" the
  // Esc key.
  Q_ASSERT(checked && actn->isCheckable());
  this->ShortCutEsc->setEnabled(true);
  this->ShortCutEsc->setProperty("PV_ACTION", QVariant::fromValue<QObject*>(actn));
}

//-----------------------------------------------------------------------------
void pqStandardViewFrameActionsImplementation::interactiveSelectionToggled(bool checked)
{
  if (!checked)
  {
    vtkSMInteractiveSelectionPipeline::GetInstance()->Hide(
      vtkSMRenderViewProxy::SafeDownCast(pqActiveObjects::instance().activeView()->getViewProxy()));
    vtkSMTooltipSelectionPipeline::GetInstance()->Hide(
      vtkSMRenderViewProxy::SafeDownCast(pqActiveObjects::instance().activeView()->getViewProxy()));
  }
}
