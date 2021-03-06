//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================
#include "smtk/extension/paraview/widgets/pqSMTKAttributeItemWidget.h"

#include "smtk/extension/qt/qtAttribute.h"
#include "smtk/extension/qt/qtBaseView.h"
#include "smtk/extension/qt/qtUIManager.h"

#include "pqInteractivePropertyWidget.h"

#include "vtkEventQtSlotConnect.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QLabel>
#include <QPointer>

/**\brief State shared by all ParaView-enabled qtItem widgets.
  *
  * ParaView has a standard API (the pqInteractivePropertyWidget)
  * for widgets that have representations in render views.
  * Instances of this class are held by the pqSMTKAttributeItemWidget
  * and used by subclasses to manage properties specific to
  * the type of ParaView widget they expose.
  */
class pqSMTKAttributeItemWidget::Internal
{
public:
  Internal(smtk::attribute::ItemPtr itm, QWidget* p, smtk::extension::qtBaseView* bview,
    Qt::Orientation orient)
    : m_orientation(orient)
    , m_overrideWhen(OverrideWhen::Unset)
    , m_geometrySource(GeometrySource::BestGuess)
    , m_fallbackStrategy(FallbackStrategy::Hide)
  {
    (void)itm;
    (void)p;
    (void)bview;
  }

  // state of this item
  QPointer<QGridLayout> m_layout;
  QPointer<QLabel> m_label;
  Qt::Orientation m_orientation;
  pqInteractivePropertyWidget* m_pvwidget;
  // pqDataRepresentation* m_pvrepr;
  vtkNew<vtkEventQtSlotConnect> m_connector;
  OverrideWhen m_overrideWhen;
  GeometrySource m_geometrySource;
  FallbackStrategy m_fallbackStrategy;

  // state of children
  QMap<QWidget*, QPair<QLayout*, QWidget*> > m_children;
};
