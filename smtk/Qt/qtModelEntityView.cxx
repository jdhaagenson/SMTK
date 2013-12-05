/*=========================================================================

Copyright (c) 1998-2003 Kitware Inc. 469 Clifton Corporate Parkway,
Clifton Park, NY, 12065, USA.

All rights reserved. No part of this software may be reproduced, distributed,
or modified, in any form or by any means, without permission in writing from
Kitware Inc.

IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY FOR
DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY DERIVATIVES THEREOF,
EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE IS PROVIDED ON AN
"AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE NO OBLIGATION TO PROVIDE
MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

=========================================================================*/

#include "smtk/Qt/qtModelEntityView.h"

#include "smtk/Qt/qtUIManager.h"
#include "smtk/Qt/qtTableWidget.h"
#include "smtk/Qt/qtAttribute.h"
#include "smtk/Qt/qtItem.h"
#include "smtk/Qt/qtAssociationWidget.h"

#include "smtk/attribute/Attribute.h"
#include "smtk/attribute/Definition.h"
#include "smtk/attribute/ItemDefinition.h"
#include "smtk/attribute/Manager.h"
#include "smtk/attribute/ValueItem.h"
#include "smtk/attribute/ValueItemDefinition.h"
#include "smtk/model/Model.h"
#include "smtk/model/Item.h"
#include "smtk/model/GroupItem.h"
#include "smtk/view/ModelEntity.h"

#include <QGridLayout>
#include <QComboBox>
#include <QTableWidgetItem>
#include <QVariant>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QKeyEvent>
#include <QModelIndex>
#include <QModelIndexList>
#include <QMessageBox>
#include <QSplitter>
#include <QPointer>

using namespace smtk::attribute;

//----------------------------------------------------------------------------
class qtModelEntityViewInternals
{
public:
  QListWidget* ListBox;
  QFrame* FiltersFrame;
  QFrame* topFrame;
  QFrame* bottomFrame;
  QPointer<qtAssociationWidget> AssociationsWidget;
  std::vector<smtk::attribute::DefinitionPtr> attDefs;
};

//----------------------------------------------------------------------------
qtModelEntityView::
qtModelEntityView(smtk::view::BasePtr dataObj, QWidget* p) :qtBaseView(dataObj, p)
{
  this->Internals = new qtModelEntityViewInternals;
  this->createWidget( );
}

//----------------------------------------------------------------------------
qtModelEntityView::~qtModelEntityView()
{
  delete this->Internals;
}

//----------------------------------------------------------------------------
const std::vector<smtk::attribute::DefinitionPtr> &qtModelEntityView::attDefinitions() const
{
  return this->Internals->attDefs;
}

//----------------------------------------------------------------------------
void qtModelEntityView::createWidget( )
{
  if(!this->getObject())
    {
    return;
    }
  smtk::view::ModelEntityPtr mview =
    smtk::dynamic_pointer_cast<smtk::view::ModelEntity>(this->getObject());
  if(!mview)
    {
    return;
    }

  Manager *attManager = qtUIManager::instance()->attManager();
  smtk::model::MaskType mask = mview->modelEntityMask();
  if(mask != 0 && !mview->definition())
    {
    attManager->findDefinitions(mask, this->Internals->attDefs);
    }

  // Create a frame to contain all gui components for this object
  // Create a list box for the group entries
  // Create a table widget
  // Add link from the listbox selection to the table widget
  // A common add/delete/(copy/paste ??) widget

  QSplitter* frame = new QSplitter(this->parentWidget());
  //this panel looks better in a over / under layout, rather than left / right
  frame->setOrientation( Qt::Vertical );

  QFrame* topFrame = new QFrame(frame);
  QFrame* bottomFrame = new QFrame(frame);

  this->Internals->topFrame = topFrame;
  this->Internals->bottomFrame = bottomFrame;

  QVBoxLayout* leftLayout = new QVBoxLayout(topFrame);
  leftLayout->setMargin(0);
  QVBoxLayout* rightLayout = new QVBoxLayout(bottomFrame);
  rightLayout->setMargin(0);

  QSizePolicy sizeFixedPolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  // create a list box for all the entries
  this->Internals->ListBox = new QListWidget(topFrame);
  this->Internals->ListBox->setSelectionMode(QAbstractItemView::SingleSelection);

  this->Internals->AssociationsWidget = new qtAssociationWidget(bottomFrame);
  rightLayout->addWidget(this->Internals->FiltersFrame);
  rightLayout->addWidget(this->Internals->AssociationsWidget);

  leftLayout->addWidget(this->Internals->ListBox);

  frame->addWidget(topFrame);
  frame->addWidget(bottomFrame);

  // if there is a definition, the view should
  // display all model entities of the requested mask along
  // with the attribute of this type in a table view
  attribute::DefinitionPtr attDef = mview->definition();
  if(attDef)
    {
    this->Internals->attDefs.push_back(attDef);
    }

  // signals/slots
  QObject::connect(this->Internals->ListBox,
    SIGNAL(currentItemChanged (QListWidgetItem * , QListWidgetItem * )),
    this, SLOT(onListBoxSelectionChanged(QListWidgetItem * , QListWidgetItem * )));

  this->Widget = frame;
  if(this->parentWidget()->layout())
    {
    this->parentWidget()->layout()->addWidget(frame);
    }
  this->updateModelAssociation();
}

//----------------------------------------------------------------------------
void qtModelEntityView::updateModelAssociation()
{
  bool isRegion = this->isRegionDomain();
  this->Internals->topFrame->setVisible(!isRegion);
  if(!isRegion)
    {
    this->updateModelItems();
    }
  this->onShowCategory();
}
//----------------------------------------------------------------------------
bool qtModelEntityView::isRegionDomain()
{
  smtk::view::ModelEntityPtr mview =
    smtk::dynamic_pointer_cast<smtk::view::ModelEntity>(this->getObject());
  if(!mview)
    {
    return false;
    }
/*
  Manager *attManager = qtUIManager::instance()->attManager();
  smtk::model::MaskType mask = mview->modelEntityMask();
  if(mask & smtk::model::Item::REGION)
    {
    return true;
    }
  attribute::DefinitionPtr attDef = mview->definition();
  if(attDef && attDef->associatesWithRegion())
    {
    return true;
    }
*/
  return false;
}

//----------------------------------------------------------------------------
void qtModelEntityView::updateModelItems()
{
  this->Internals->ListBox->blockSignals(true);
  this->Internals->ListBox->clear();

  smtk::view::ModelEntityPtr mview =
    smtk::dynamic_pointer_cast<smtk::view::ModelEntity>(this->getObject());
  if(!mview)
    {
    this->Internals->ListBox->blockSignals(false);
    return;
    }
  if(smtk::model::MaskType mask = mview->modelEntityMask())
    {
    smtk::model::ModelPtr refModel = qtUIManager::instance()->attManager()->refModel();
    std::vector<smtk::model::GroupItemPtr> result=refModel->findGroupItems(mask);
    std::vector<smtk::model::GroupItemPtr>::iterator it = result.begin();
    for(; it!=result.end(); ++it)
      {
      this->addModelItem(*it);
      }
    }
  if(this->Internals->ListBox->count())
    {
    this->Internals->ListBox->setCurrentRow(0);
    }
  this->Internals->ListBox->blockSignals(false);
}

//----------------------------------------------------------------------------
void qtModelEntityView::onShowCategory()
{
  if(this->isRegionDomain())
    {
    smtk::view::ModelEntityPtr mview =
      smtk::dynamic_pointer_cast<smtk::view::ModelEntity>(this->getObject());
    smtk::model::MaskType mask = mview->modelEntityMask() ?
      mview->modelEntityMask() : smtk::model::Item::REGION;
    smtk::model::ModelPtr refModel = qtUIManager::instance()->attManager()->refModel();
    std::vector<smtk::model::GroupItemPtr> result(refModel->findGroupItems(mask));
    this->Internals->AssociationsWidget->showDomainsAssociation(
      result, this->Internals->attDefs);
    }
  else
    {
    smtk::model::ItemPtr theItem = this->getSelectedModelItem();
    if(theItem)
      {
      this->Internals->AssociationsWidget->showAttributeAssociation(
        theItem, this->Internals->attDefs);
      }
    }
}
//----------------------------------------------------------------------------
void qtModelEntityView::onListBoxSelectionChanged(
  QListWidgetItem * current, QListWidgetItem * previous)
{
  this->onShowCategory();
}
//-----------------------------------------------------------------------------
smtk::model::ItemPtr qtModelEntityView::getSelectedModelItem()
{
  return this->getModelItem(this->getSelectedItem());
}
//-----------------------------------------------------------------------------
smtk::model::ItemPtr qtModelEntityView::getModelItem(
  QListWidgetItem * item)
{
  smtk::model::Item* rawPtr = item ?
    static_cast<smtk::model::Item*>(item->data(Qt::UserRole).value<void *>()) : NULL;
  return rawPtr ? rawPtr->pointer() : smtk::model::ItemPtr();
}
//-----------------------------------------------------------------------------
QListWidgetItem *qtModelEntityView::getSelectedItem()
{
  return this->Internals->ListBox->currentItem();
}
//----------------------------------------------------------------------------
QListWidgetItem* qtModelEntityView::addModelItem(
  smtk::model::ItemPtr childData)
{
  QListWidgetItem* item = new QListWidgetItem(
      QString::fromUtf8(childData->name().c_str()),
      this->Internals->ListBox, smtk_USER_DATA_TYPE);
  QVariant vdata;
  vdata.setValue((void*)(childData.get()));
  item->setData(Qt::UserRole, vdata);
//  item->setFlags(item->flags() | Qt::ItemIsEditable);
  this->Internals->ListBox->addItem(item);
  return item;
}
