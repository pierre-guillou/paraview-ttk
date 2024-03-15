// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-FileCopyrightText: Copyright (c) Sandia Corporation
// SPDX-License-Identifier: BSD-3-Clause
#ifndef pqUseSeparateColorMapReaction_h
#define pqUseSeparateColorMapReaction_h

#include "pqPropertyLinks.h" // For Links
#include "pqReaction.h"
#include <QPointer>

class pqDataRepresentation;
class pqDisplayColorWidget;

/**
 * @ingroup Reactions
 * Reaction to toggle the use of separated color map for an array in a representation
 */
class PQAPPLICATIONCOMPONENTS_EXPORT pqUseSeparateColorMapReaction : public pqReaction
{
  Q_OBJECT
  typedef pqReaction Superclass;

public:
  /**
   * if \c track_active_objects is false, then the reaction will not track
   * pqActiveObjects automatically.
   * colorWidget is used to force a representation update
   */
  pqUseSeparateColorMapReaction(
    QAction* parent, pqDisplayColorWidget* colorWidget, bool track_active_objects = true);
  ~pqUseSeparateColorMapReaction() override;

  /**
   * Returns the representation currently being used by the reaction.
   */
  pqDataRepresentation* representation() const;

public Q_SLOTS: // NOLINT(readability-redundant-access-specifiers)
  /**
   * Set the active representation.
   */
  void setRepresentation(pqDataRepresentation*);

protected Q_SLOTS:
  /**
   * Updates the enabled state. Applications need not explicitly call this.
   */
  void updateEnableState() override;

  /**
   * Called when the action is triggered.
   */
  void onTriggered() override;

private:
  Q_DISABLE_COPY(pqUseSeparateColorMapReaction)

  pqPropertyLinks Links;
  QPointer<pqDataRepresentation> CachedRepresentation;
  pqDisplayColorWidget* ColorWidget;
  bool TrackActiveObjects;
  bool BlockSignals;
};

#endif
