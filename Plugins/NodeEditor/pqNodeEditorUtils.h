/*=========================================================================

  Program:   ParaView
  Plugin:    NodeEditor

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/*-------------------------------------------------------------------------
  ParaViewPluginsNodeEditor - BSD 3-Clause License - Copyright (C) 2021 Jonas Lukasczyk

  See the Copyright.txt file provided
  with ParaViewPluginsNodeEditor for license information.
-------------------------------------------------------------------------*/

#ifndef pqNodeEditorUtils_h
#define pqNodeEditorUtils_h

#include <QApplication>
#include <QColor>
#include <QObject>
#include <QPalette>

#include "vtkType.h"

class pqProxy;

namespace pqNodeEditorUtils
{
namespace CONSTS
{
// UI dimensions
constexpr int PORT_RADIUS = 8;
constexpr int PORT_HEIGHT = 20;
constexpr int PORT_PADDING = 1;
constexpr qreal PORT_LABEL_OFFSET = 3.0;

constexpr int NODE_WIDTH = 300;
constexpr int NODE_BORDER_WIDTH = 4;
constexpr int NODE_FONT_SIZE = 13;
constexpr int NODE_LABEL_HEIGHT = 30;
constexpr int NODE_HEADLINE_MIN_HEIGHT = 1.5 * NODE_LABEL_HEIGHT;

constexpr int EDGE_WIDTH = 4;
constexpr int EDGE_OUTLINE = 1;

constexpr qreal GRID_SIZE = 25;

// UI colors
const QColor COLOR_BASE = QApplication::palette().window().color();
const QColor COLOR_GRID = QApplication::palette().mid().color();
const QColor COLOR_HIGHLIGHT = QApplication::palette().highlight().color();
const QColor COLOR_BASE_DEEP = COLOR_BASE.lighter(COLOR_BASE.lightness() * 0.7 + 10);
const QColor COLOR_CONSTRAST = QColor::fromHslF(
  COLOR_BASE.hueF(), COLOR_BASE.saturationF(), 0.5 + 0.2 * (COLOR_BASE.lightnessF() - 0.5));
const QColor COLOR_BASE_GREEN = QColor::fromHslF(0.361, 0.666, COLOR_BASE.lightnessF() * 0.4 + 0.2);
const QColor COLOR_BASE_ORANGE = QColor::fromHslF(0.07, 0.666, COLOR_HIGHLIGHT.lightnessF());
const QColor COLOR_DULL_ORANGE = QColor::fromHslF(
  COLOR_BASE_ORANGE.hueF(), COLOR_BASE_ORANGE.saturationF() * 0.4, COLOR_CONSTRAST.lightnessF());

// Z depth for graph elements
constexpr int NODE_LAYER = 10;
constexpr int EDGE_LAYER = 20;
constexpr int PORT_LAYER = 30;
constexpr int FOREGROUND_LAYER = 40;
constexpr int VIEW_NODE_LAYER = 50;
};

template <typename F>
/**
 * Intercept all events from a particular QObject and process them using the
 * given @c functor. This is usually used with the QObjects::installEventFilter()
 * function.
 */
class Interceptor final : public QObject
{
public:
  /**
   * Create an Interceptor that process all events of @c parent using @c functor.
   */
  Interceptor(QObject* parent, F fn)
    : QObject(parent)
    , functor(fn)
  {
  }
  ~Interceptor() override = default;

protected:
  /**
   * Filters events if this object has been installed as an event filter for the watched object.
   */
  bool eventFilter(QObject* object, QEvent* event) override { return this->functor(object, event); }

  F functor;
};

/**
 * Create a new Interceptor instance.
 */
template <typename F>
Interceptor<F>* createInterceptor(QObject* parent, F functor)
{
  return new Interceptor<F>(parent, functor);
};

vtkIdType getID(pqProxy* proxy);

std::string getLabel(pqProxy* proxy);
};

#endif // pqNodeEditorUtils_h
