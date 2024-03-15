// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-License-Identifier: BSD-3-Clause
#include "pqExampleVisualizationsDialog.h"
#include "ui_pqExampleVisualizationsDialog.h"

#include "pqActiveObjects.h"
#include "pqApplicationCore.h"
#include "pqEventDispatcher.h"
#include "pqLoadStateReaction.h"
#include "vtkPVFileInformation.h"

#include <QFile>
#include <QFileInfo>
#include <QMessageBox>

#include <cassert>

//-----------------------------------------------------------------------------
pqExampleVisualizationsDialog::pqExampleVisualizationsDialog(QWidget* parentObject)
  : Superclass(parentObject)
  , ui(new Ui::pqExampleVisualizationsDialog)
{
  ui->setupUi(this);
  // hide the Context Help item (it's a "?" in the Title Bar for Windows, a menu item for Linux)
  this->setWindowFlags(this->windowFlags().setFlag(Qt::WindowContextHelpButtonHint, false));

  QObject::connect(
    this->ui->Example1Button, SIGNAL(clicked(bool)), this, SLOT(onButtonPressed()));
  QObject::connect(
    this->ui->Example2Button, SIGNAL(clicked(bool)), this, SLOT(onButtonPressed()));
  QObject::connect(
    this->ui->Example3Button, SIGNAL(clicked(bool)), this, SLOT(onButtonPressed()));
}

//-----------------------------------------------------------------------------
pqExampleVisualizationsDialog::~pqExampleVisualizationsDialog()
{
  delete ui;
}

//-----------------------------------------------------------------------------
void pqExampleVisualizationsDialog::onButtonPressed()
{
  QString dataPath(vtkPVFileInformation::GetParaViewExampleFilesDirectory().c_str());
  QPushButton* button = qobject_cast<QPushButton*>(sender());
  if (button)
  {
    const char* stateFile = nullptr;
    bool needsData = false;
    if (button == this->ui->Example1Button)
    {
      stateFile = ":/pqApplicationComponents/ExampleVisualizations/Example1.pvsm";
      needsData = true;
    }
    else if (button == this->ui->Example2Button)
    {
      stateFile = ":/pqApplicationComponents/ExampleVisualizations/Example2.pvsm";
      needsData = true;
    }
    else if (button == this->ui->Example3Button)
    {
      stateFile = ":/pqApplicationComponents/ExampleVisualizations/Example3.pvsm";
      needsData = false;
    }
    else
    {
      qCritical("No example file for button");
      return;
    }
    if (needsData)
    {
      QFileInfo fdataPath(dataPath);
      if (!fdataPath.isDir())
      {
        QString msg =
          tr("Your installation doesn't have datasets to load the example visualizations. "
             "You can manually download the datasets from paraview.org and then "
             "place them under the following path for examples to work:\n\n'%1'")
            .arg(fdataPath.absoluteFilePath());
        // dump to cout for easy copy/paste.
        cout << msg.toUtf8().data() << endl;
        QMessageBox::warning(this, tr("Missing data"), msg, QMessageBox::Ok);
        return;
      }
      dataPath = fdataPath.absoluteFilePath();
    }

    this->hide();
    assert(stateFile != nullptr);

    QFile qfile(stateFile);
    if (qfile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
      QMessageBox box(this);
      box.setText(tr("Loading example visualization, please wait ..."));
      box.setStandardButtons(QMessageBox::NoButton);
      box.show();

      // without this, the message box doesn't paint properly.
      pqEventDispatcher::processEventsAndWait(100);

      QString xmldata(qfile.readAll());
      xmldata.replace("$PARAVIEW_EXAMPLES_DATA", dataPath);
      pqApplicationCore::instance()->loadStateFromString(
        xmldata.toUtf8().data(), pqActiveObjects::instance().activeServer());

      // This is needed since XML state currently does not save active view.
      pqLoadStateReaction::activateView();
    }
    else
    {
      qCritical("Failed to open resource");
    }
  }
}
