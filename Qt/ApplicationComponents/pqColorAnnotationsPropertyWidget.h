/*=========================================================================

   Program: ParaView
   Module:  pqColorAnnotationsPropertyWidget.h

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
#ifndef pqColorAnnotationsPropertyWidget_h
#define pqColorAnnotationsPropertyWidget_h

#include "pqApplicationComponentsModule.h"
#include "pqPropertyWidget.h"

class QItemSelection;
class vtkSMPropertyGroup;

/**
 * pqColorAnnotationsPropertyWidget is used to edit the Annotations property on the
 * "PVLookupTable" proxy. The property group can comprise of two properties,
 * \c Annotations and \c IndexedColors.
 */
class PQAPPLICATIONCOMPONENTS_EXPORT pqColorAnnotationsPropertyWidget : public pqPropertyWidget
{
  Q_OBJECT;
  Q_PROPERTY(QList<QVariant> annotations READ annotations WRITE setAnnotations);
  Q_PROPERTY(QList<QVariant> indexedColors READ indexedColors WRITE setIndexedColors);
  Q_PROPERTY(QList<QVariant> indexedOpacities READ indexedOpacities WRITE setIndexedOpacities);
  Q_PROPERTY(QVariant opacityMapping READ opacityMapping WRITE setOpacityMapping);

  typedef pqPropertyWidget Superclass;

public:
  pqColorAnnotationsPropertyWidget(
    vtkSMProxy* proxy, vtkSMPropertyGroup* smgroup, QWidget* parent = nullptr);
  ~pqColorAnnotationsPropertyWidget() override;

  //@{
  /**
   * Get/Set the annotations.
   * This is a list generated by flattening 2-tuples where 1st value is the
   * annotated value and second is the annotation text.
   */
  QList<QVariant> annotations() const;
  void setAnnotations(const QList<QVariant>&);
  //@}

  //@{
  /**
   * Get/Set the indexed colors. This is a list generated by flattening
   * 3-tuples (r,g,b).
   */
  QList<QVariant> indexedColors() const;
  void setIndexedColors(const QList<QVariant>&);
  //@}

  //@{
  /**
   * Get/Set the indexed opacities.
   */
  QList<QVariant> indexedOpacities() const;
  void setIndexedOpacities(const QList<QVariant>&);
  //@}

  //@{
  /**
   * Get/Set the opacity mapping status
   */
  QVariant opacityMapping() const;
  void setOpacityMapping(const QVariant&);
  //@}

Q_SIGNALS:
  /**
   * Fired when the annotations are changed.
   */
  void annotationsChanged();

  /**
   * Fired when the indexed colors are changed.
   */
  void indexedColorsChanged();

  /**
   * Fired when the indexed opacities are changed.
   */
  void indexedOpacitiesChanged();

  /**
   * Fired when the opacity mapping is changed.
   */
  void opacityMappingChanged();

private Q_SLOTS:

  /**
   * Ensures that the table for indexedColors are shown only when this
   * is set to true.
   */
  void updateIndexedLookupState();

private: // NOLINT(readability-redundant-access-specifiers)
  Q_DISABLE_COPY(pqColorAnnotationsPropertyWidget)

  class pqInternals;
  pqInternals* Internals;
};

#endif
