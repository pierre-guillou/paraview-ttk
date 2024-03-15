// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-License-Identifier: BSD-3-Clause
#ifndef pqColorEditorPropertyWidget_h
#define pqColorEditorPropertyWidget_h

#include "pqApplicationComponentsModule.h"
#include "pqPropertyWidget.h"

/**
 * This is a pqPropertyWidget subclass that presents a widget to edit the color
 * of a representation and other related functionality. It's used as the
 * "widget" for \c ColorEditor property group.
 */
class PQAPPLICATIONCOMPONENTS_EXPORT pqColorEditorPropertyWidget : public pqPropertyWidget
{
  Q_OBJECT
public:
  typedef pqPropertyWidget Superclass;

  pqColorEditorPropertyWidget(vtkSMProxy* proxy, QWidget* parent = nullptr);
  ~pqColorEditorPropertyWidget() override;

private Q_SLOTS:
  void updateEnableState();

private: // NOLINT(readability-redundant-access-specifiers)
  class pqInternals;
  pqInternals* Internals;

  Q_DISABLE_COPY(pqColorEditorPropertyWidget)
};

#endif // pqColorEditorPropertyWidget_h
