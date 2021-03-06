//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================

#include "smtk/attribute/Attribute.h"
#include "smtk/attribute/ComponentItem.h"
#include "smtk/attribute/DateTimeItem.h"
#include "smtk/attribute/Definition.h"
#include "smtk/attribute/DirectoryItem.h"
#include "smtk/attribute/DoubleItem.h"
#include "smtk/attribute/FileItem.h"
#include "smtk/attribute/GroupItem.h"
#include "smtk/attribute/IntItem.h"
#include "smtk/attribute/Item.h"
#include "smtk/attribute/MeshItem.h"
#include "smtk/attribute/MeshSelectionItem.h"
#include "smtk/attribute/ModelEntityItem.h"
#include "smtk/attribute/ModelEntityItemDefinition.h"
#include "smtk/attribute/RefItem.h"
#include "smtk/attribute/Resource.h"
#include "smtk/attribute/ResourceItem.h"
#include "smtk/attribute/StringItem.h"
#include "smtk/attribute/ValueItem.h"
#include "smtk/attribute/VoidItem.h"

#include "smtk/model/EntityRef.h"
#include "smtk/model/Resource.h"

#include "smtk/resource/Manager.h"

#include "smtk/common/CompilerInformation.h"
#include "smtk/common/UUIDGenerator.h"

SMTK_THIRDPARTY_PRE_INCLUDE
#include "boost/algorithm/string.hpp"
SMTK_THIRDPARTY_POST_INCLUDE

#include <cassert>
#include <functional>
#include <iostream>

using namespace smtk::attribute;
using namespace smtk::common;

Attribute::Attribute(const std::string& myName, smtk::attribute::DefinitionPtr myDefinition,
  const smtk::common::UUID& myId)
  : Component()
  , m_name(myName)
  , m_definition(myDefinition)
  , m_appliesToBoundaryNodes(false)
  , m_appliesToInteriorNodes(false)
  , m_isColorSet(false)
  , m_aboutToBeDeleted(false)
  , m_id(myId)
{
  m_definition->buildAttribute(this);
}

Attribute::Attribute(const std::string& myName, smtk::attribute::DefinitionPtr myDefinition)
  : Component()
  , m_name(myName)
  , m_definition(myDefinition)
  , m_appliesToBoundaryNodes(false)
  , m_appliesToInteriorNodes(false)
  , m_isColorSet(false)
  , m_aboutToBeDeleted(false)
  , m_id(smtk::common::UUIDGenerator::instance().random())
  , m_includeIndex(0)
{
  m_definition->buildAttribute(this);
}

Attribute::~Attribute()
{
  m_aboutToBeDeleted = true;
  // Clear all references to the attribute
  std::map<smtk::attribute::RefItem*, std::set<std::size_t> >::iterator it;
  for (it = m_references.begin(); it != m_references.end(); it++)
  {
    std::set<std::size_t>::iterator sit;
    for (sit = it->second.begin(); sit != it->second.end(); sit++)
    {
      it->first->unset(*sit);
    }
  }
  this->removeAllItems();
}

void Attribute::removeAllItems()
{
  // we need to detatch all items owned by this attribute
  std::size_t i, n = m_items.size();
  for (i = 0; i < n; i++)
  {
    m_items[i]->detachOwningAttribute();
  }
  m_items.clear();
}

void Attribute::references(std::vector<smtk::attribute::ItemPtr>& list) const
{
  list.clear();
  std::map<smtk::attribute::RefItem*, std::set<std::size_t> >::const_iterator it;
  for (it = m_references.begin(); it != m_references.end(); it++)
  {
    if (it->second.size())
    {
      list.push_back(it->first->shared_from_this());
    }
  }
}

const double* Attribute::color() const
{
  if (m_isColorSet)
  {
    return m_color;
  }
  return m_definition->defaultColor();
}

const std::string& Attribute::type() const
{
  return m_definition->type();
}

std::vector<std::string> Attribute::types() const
{
  std::vector<std::string> tvec;
  smtk::attribute::DefinitionPtr def = m_definition;
  while (def)
  {
    tvec.push_back(def->type());
    def = def->baseDefinition();
  }
  return tvec;
}

bool Attribute::isA(smtk::attribute::DefinitionPtr def) const
{
  return m_definition->isA(def);
}

bool Attribute::isMemberOf(const std::string& category) const
{
  return m_definition->isMemberOf(category);
}

bool Attribute::isMemberOf(const std::vector<std::string>& categories) const
{
  return m_definition->isMemberOf(categories);
}

/**\brief Return an item given a string specifying a path to it.
  *
  */
smtk::attribute::ConstItemPtr Attribute::itemAtPath(
  const std::string& path, const std::string& seps) const
{
  smtk::attribute::ConstItemPtr result;
  std::vector<std::string> tree;
  std::vector<std::string>::iterator it;
  boost::split(tree, path, boost::is_any_of(seps));
  if (tree.empty())
  {
    return result;
  }

  it = tree.begin();
  smtk::attribute::ConstItemPtr current = this->find(*it, NO_CHILDREN);
  if (current)
  {
    bool ok = true;
    for (++it; it != tree.end(); ++it)
    {
      ConstValueItemPtr vitm = smtk::dynamic_pointer_cast<const ValueItem>(current);
      ConstGroupItemPtr gitm = smtk::dynamic_pointer_cast<const GroupItem>(current);
      if (vitm && (current = vitm->findChild(*it, NO_CHILDREN)))
        continue; // OK, keep descending
      else if (gitm && (current = gitm->find(*it)))
        continue; // OK, keep descending
      else
      {
        ok = false;
        break;
      }
    }
    if (ok)
    {
      result = current;
    }
  }
  return result;
}

smtk::attribute::ItemPtr Attribute::itemAtPath(const std::string& path, const std::string& seps)
{
  smtk::attribute::ItemPtr result;
  std::vector<std::string> tree;
  std::vector<std::string>::iterator it;
  boost::split(tree, path, boost::is_any_of(seps));
  if (tree.empty())
  {
    return result;
  }

  it = tree.begin();
  smtk::attribute::ItemPtr current = this->find(*it, NO_CHILDREN);
  if (current)
  {
    bool ok = true;
    for (++it; it != tree.end(); ++it)
    {
      ValueItemPtr vitm = smtk::dynamic_pointer_cast<ValueItem>(current);
      GroupItemPtr gitm = smtk::dynamic_pointer_cast<GroupItem>(current);
      if (vitm && (current = vitm->findChild(*it, NO_CHILDREN)))
      {
        continue; // OK, keep descending
      }
      else if (gitm && (current = gitm->find(*it)))
      {
        continue; // OK, keep descending
      }
      else
      {
        ok = false;
        break;
      }
    }
    if (ok)
    {
      result = current;
    }
  }
  return result;
}

/**\brief Validate the attribute against its definition.
  *
  * This method will only return true when every (required) item in the
  * attribute is set and considered a valid value by its definition.
  * This can be used to ensure that an attribute is in a good state
  * before using it to perform some operation.
  */
bool Attribute::isValid() const
{
  for (auto it = m_items.begin(); it != m_items.end(); ++it)
  {
    if (!(*it)->isValid())
    {
      return false;
    }
  }
  // also check associations
  if (m_associatedObjects && !m_associatedObjects->isValid())
  {
    return false;
  }
  return true;
}

ResourcePtr Attribute::attributeResource() const
{
  return m_definition->resource();
}

const smtk::resource::ResourcePtr Attribute::resource() const
{
  return this->attributeResource();
}

/**\brief Remove all associations of this attribute with model entities.
  *
  * Note that this actually resets the associations.
  * If there are any default associations, they will be present
  * but typically there are none.
  */
void Attribute::removeAllAssociations()
{
  if (m_associatedObjects)
  {
    for (auto oit = m_associatedObjects->begin(); oit != m_associatedObjects->end(); ++oit)
    {
      auto modelEnt = dynamic_pointer_cast<smtk::model::Entity>(*oit);
      if (modelEnt)
      {
        modelEnt->modelResource()->disassociateAttribute(
          nullptr, this->id(), modelEnt->id(), false);
      }
    }
    m_associatedObjects->reset();
  }
}

/**\brief Update attribute when entities has been expunged
  * Note it would check associations and every modelEntityItem
  */
bool Attribute::removeExpungedEntities(const smtk::model::EntityRefs& expungedEnts)
{
  bool associationChanged = false;
  // update all modelEntityItems
  std::set<smtk::attribute::ModelEntityItemPtr> modelEntityPtrs;
  std::function<bool(smtk::attribute::ModelEntityItemPtr)> filter = [](
    smtk::attribute::ModelEntityItemPtr) { return true; };
  this->filterItems(modelEntityPtrs, filter, false);
  for (std::set<smtk::attribute::ModelEntityItemPtr>::iterator iterator = modelEntityPtrs.begin();
       iterator != modelEntityPtrs.end(); iterator++)
  {
    smtk::attribute::ModelEntityItemPtr MEItem = *iterator;
    if (MEItem && MEItem->isValid())
    {
      for (smtk::model::EntityRefs::const_iterator bit = expungedEnts.begin();
           bit != expungedEnts.end(); ++bit)
      {
        std::ptrdiff_t idx = MEItem->find(*bit);
        if (idx >= 0)
        {
          MEItem->removeValue(static_cast<std::size_t>(idx));
          associationChanged = true;
        }
      }
    }
  }
  if (this->associations())
  {
    for (smtk::model::EntityRefs::const_iterator bit = expungedEnts.begin();
         bit != expungedEnts.end(); ++bit)
    {
      if (this->isEntityAssociated(*bit))
      {
        this->disassociateEntity(*bit);
        associationChanged = true;
      }
    }
  }
  return associationChanged;
}

bool Attribute::isObjectAssociated(const smtk::common::UUID& entity) const
{
  return m_associatedObjects ? m_associatedObjects->has(entity) : false;
}

bool Attribute::isObjectAssociated(const smtk::resource::PersistentObjectPtr& comp) const
{
  return m_associatedObjects ? m_associatedObjects->has(comp) : false;
}

/**\brief Is the model \a entity associated with this attribute?
  *
  */
bool Attribute::isEntityAssociated(const smtk::common::UUID& entity) const
{
  return m_associatedObjects ? m_associatedObjects->has(entity) : false;
}

/**\brief Is the model entity of the \a entityref associated with this attribute?
  *
  */
bool Attribute::isEntityAssociated(const smtk::model::EntityRef& entityref) const
{
  auto comp = entityref.component();
  return (comp && m_associatedObjects) ? m_associatedObjects->has(comp) : false;
}

/**\brief Return the associated model entities as a set of UUIDs.
  *
  */
smtk::common::UUIDs Attribute::associatedModelEntityIds() const
{
  smtk::common::UUIDs result;
  auto assoc = this->associations();
  if (assoc)
  {
    for (std::size_t i = 0; i < assoc->numberOfValues(); ++i)
    {
      if (assoc->isSet(i))
      {
        auto key = assoc->objectKey(i);
        result.insert(this->resource()->links().data().at(key.first).at(key.second).right);
      }
    }
  }
  return result;
}

/*! \fn template<typename T> T Attribute::associatedModelEntities() const
 *\brief Return a container of associated entityref-subclass instances.
 *
 * This method returns a container (usually a std::vector or std::set) of
 * entityref-subclass instances (e.g., Edge, EdgeUse, Loop) that are
 * associated with this attribute.
 *
 * Note that if you request a container of EntityRef entities, you will obtain
 * <b>all</b> of the associated model entities. However, if you request
 * a container of some subclass, only entities of that type will be returned.
 * For example, if an attribute is associated with two faces, an edge,
 * a group, and a shell, calling `associatedModelEntities<EntityRefs>()` will
 * return 5 EntityRef entries while `associatedModelEntities<CellEntities>()`
 * will return 3 entries (2 faces and 1 edge) since the other entities
 * do not construct valid CellEntity instances.
 */

bool Attribute::associate(smtk::resource::PersistentObjectPtr obj)
{
  bool res = this->isObjectAssociated(obj);
  if (res)
  {
    return res;
  }

  res = m_associatedObjects ? m_associatedObjects->appendObjectValue(obj) : false;
  if (!res)
  {
    return res;
  }

  auto modelEnt = std::dynamic_pointer_cast<smtk::model::Entity>(obj);
  if (modelEnt)
  {
    res = modelEnt->modelResource()->associateAttribute(nullptr, this->id(), modelEnt->id());
  }
  return res;
}

/**\brief Associate a new-style model ID (a UUID) with this attribute.
  *
  * This function returns true when the association is valid and
  * successful. It will return false if the association is prohibited.
  */
bool Attribute::associateEntity(const smtk::common::UUID& objId)
{
  std::set<smtk::resource::Resource::Ptr> rsrcs = this->attributeResource()->associations();
  rsrcs.insert(this->attributeResource()); // We can always look for other attributes.

  // If we have a resource manager, we can also look for components in other resources:
  auto rsrcMgr = this->attributeResource()->manager();
  if (rsrcMgr)
  {
    std::for_each(rsrcMgr->resources().begin(), rsrcMgr->resources().end(),
      [&rsrcs](smtk::resource::Resource::Ptr rsrc) { rsrcs.insert(rsrc); });
  }

  // Look for anything with the given UUID:
  for (auto rsrc : rsrcs)
  {
    if (rsrc != nullptr)
    {
      if (rsrc->id() == objId)
      {
        return this->associate(rsrc);
      }
      auto comp = rsrc->find(objId);
      if (comp)
      {
        return this->associate(comp);
      }
    }
  }
  return false;
}

/**\brief Associate a new-style model ID (a EntityRef) with this attribute.
  *
  * This function returns true when the association is valid and
  * successful. It may return false if the association is prohibited.
  * (This is not currently implemented.)
  */
bool Attribute::associateEntity(const smtk::model::EntityRef& entityRef)
{
  bool res = this->isEntityAssociated(entityRef);
  if (res)
    return res;

  res =
    (m_associatedObjects) ? m_associatedObjects->appendObjectValue(entityRef.component()) : false;
  if (!res)
    return res;

  smtk::model::ResourcePtr modelMgr = entityRef.resource();
  if (modelMgr)
  {
    res = modelMgr->associateAttribute(nullptr, this->id(), entityRef.entity());
  }
  return res;
}

/**\brief Disassociate a new-style model ID (a UUID) from this attribute.
  *
  * If \a reverse is true (the default), then the model resource is notified
  * of the item's disassociation immediately after its removal from this
  * attribute, allowing the model and attribute to stay in sync.
  */
void Attribute::disassociateEntity(const smtk::common::UUID& entity, bool reverse)
{
  if (!m_associatedObjects)
  {
    return;
  }

  std::set<smtk::resource::Resource::Ptr> rsrcs = this->attributeResource()->associations();
  rsrcs.insert(this->attributeResource()); // We can always look for other attributes.

  // If we have a resource manager, we can also look for components in other resources:
  auto rsrcMgr = this->attributeResource()->manager();
  if (rsrcMgr)
  {
    std::for_each(rsrcMgr->resources().begin(), rsrcMgr->resources().end(),
      [&rsrcs](smtk::resource::Resource::Ptr rsrc) { rsrcs.insert(rsrc); });
  }

  // Look for anything with the given UUID:
  for (auto rsrc : rsrcs)
  {
    if (rsrc != nullptr)
    {
      auto comp = rsrc->find(entity);
      if (comp)
      {
        this->disassociate(comp);

        if (reverse)
        {
          smtk::model::ResourcePtr modelMgr = std::static_pointer_cast<smtk::model::Resource>(rsrc);
          if (modelMgr)
          {
            modelMgr->disassociateAttribute(this->attributeResource(), this->id(), entity, false);
          }
        }
      }
    }
  }
}

/**\brief Disassociate a new-style model entity (a EntityRef) from this attribute.
  *
  */
void Attribute::disassociateEntity(const smtk::model::EntityRef& entity, bool reverse)
{
  if (!m_associatedObjects)
  {
    return;
  }

  std::ptrdiff_t idx = m_associatedObjects->find(entity.entity());
  if (idx >= 0)
  {
    m_associatedObjects->removeValue(idx);
    if (reverse)
    {
      smtk::model::EntityRef mutableEntity(entity);
      mutableEntity.disassociateAttribute(this->attributeResource(), this->id(), false);
    }
  }
}

void Attribute::disassociate(smtk::resource::PersistentObjectPtr obj, bool reverse)
{
  if (!m_associatedObjects)
  {
    return;
  }

  std::ptrdiff_t idx = m_associatedObjects->find(obj);
  if (idx >= 0)
  {
    m_associatedObjects->removeValue(idx);
    if (reverse)
    {
      auto modelEnt = std::dynamic_pointer_cast<smtk::model::Entity>(obj);
      if (modelEnt)
      {
        modelEnt->modelResource()->disassociateAttribute(
          this->attributeResource(), this->id(), modelEnt->id(), false);
      }
    }
  }
}

/**\brief Return the item with the given \a inName, searching in the given \a style.
  *
  * The search style dictates whether children of conditional items are included
  * and, if so, whether all of their children are searched or just the active children.
  * The default is to search active children.
  */
smtk::attribute::ItemPtr Attribute::find(const std::string& inName, SearchStyle style)
{
  int i = m_definition->findItemPosition(inName);
  if (i < 0 && style != NO_CHILDREN)
  { // try to find child items that match the name and type.
    std::size_t numItems = this->numberOfItems();
    for (i = 0; i < static_cast<int>(numItems); ++i)
    {
      ValueItem::Ptr vitem = dynamic_pointer_cast<ValueItem>(this->item(i));
      Item::Ptr match;
      if (vitem && (match = vitem->findChild(inName, style)))
      {
        return match;
      }
    }
    i = -1; // Nothing found.
  }
  assert(i < 0 || m_items.size() > static_cast<std::size_t>(i));
  return (i < 0) ? smtk::attribute::ItemPtr() : m_items[static_cast<std::size_t>(i)];
}

smtk::attribute::ConstItemPtr Attribute::find(const std::string& inName, SearchStyle style) const
{
  int i = m_definition->findItemPosition(inName);
  if (i < 0 && style != NO_CHILDREN)
  { // try to find child items that match the name and type.
    std::size_t numItems = this->numberOfItems();
    for (i = 0; i < static_cast<int>(numItems); ++i)
    {
      ConstValueItemPtr vitem = dynamic_pointer_cast<const ValueItem>(this->item(i));
      ConstItemPtr match;
      if (vitem && (match = vitem->findChild(inName, style)))
      {
        return match;
      }
    }
    i = -1; // Nothing found.
  }
  assert(i < 0 || m_items.size() > static_cast<std::size_t>(i));
  return (i < 0) ? smtk::attribute::ConstItemPtr()
                 : smtk::const_pointer_cast<const Item>(m_items[static_cast<std::size_t>(i)]);
}

smtk::attribute::IntItemPtr Attribute::findInt(const std::string& nameStr)
{
  return smtk::dynamic_pointer_cast<IntItem>(this->find(nameStr));
}
smtk::attribute::ConstIntItemPtr Attribute::findInt(const std::string& nameStr) const
{
  return smtk::dynamic_pointer_cast<const IntItem>(this->find(nameStr));
}

smtk::attribute::DoubleItemPtr Attribute::findDouble(const std::string& nameStr)
{
  return smtk::dynamic_pointer_cast<DoubleItem>(this->find(nameStr));
}
smtk::attribute::ConstDoubleItemPtr Attribute::findDouble(const std::string& nameStr) const
{
  return smtk::dynamic_pointer_cast<const DoubleItem>(this->find(nameStr));
}

smtk::attribute::StringItemPtr Attribute::findString(const std::string& nameStr)
{
  return smtk::dynamic_pointer_cast<StringItem>(this->find(nameStr));
}
smtk::attribute::ConstStringItemPtr Attribute::findString(const std::string& nameStr) const
{
  return smtk::dynamic_pointer_cast<const StringItem>(this->find(nameStr));
}

smtk::attribute::FileItemPtr Attribute::findFile(const std::string& nameStr)
{
  return smtk::dynamic_pointer_cast<FileItem>(this->find(nameStr));
}
smtk::attribute::ConstFileItemPtr Attribute::findFile(const std::string& nameStr) const
{
  return smtk::dynamic_pointer_cast<const FileItem>(this->find(nameStr));
}

smtk::attribute::DirectoryItemPtr Attribute::findDirectory(const std::string& nameStr)
{
  return smtk::dynamic_pointer_cast<DirectoryItem>(this->find(nameStr));
}
smtk::attribute::ConstDirectoryItemPtr Attribute::findDirectory(const std::string& nameStr) const
{
  return smtk::dynamic_pointer_cast<const DirectoryItem>(this->find(nameStr));
}

smtk::attribute::GroupItemPtr Attribute::findGroup(const std::string& nameStr)
{
  return smtk::dynamic_pointer_cast<GroupItem>(this->find(nameStr));
}
smtk::attribute::ConstGroupItemPtr Attribute::findGroup(const std::string& nameStr) const
{
  return smtk::dynamic_pointer_cast<const GroupItem>(this->find(nameStr));
}

smtk::attribute::RefItemPtr Attribute::findRef(const std::string& nameStr)
{
  return smtk::dynamic_pointer_cast<RefItem>(this->find(nameStr));
}
smtk::attribute::ConstRefItemPtr Attribute::findRef(const std::string& nameStr) const
{
  return smtk::dynamic_pointer_cast<const RefItem>(this->find(nameStr));
}

smtk::attribute::ModelEntityItemPtr Attribute::findModelEntity(const std::string& nameStr)
{
  return smtk::dynamic_pointer_cast<ModelEntityItem>(this->find(nameStr));
}
smtk::attribute::ConstModelEntityItemPtr Attribute::findModelEntity(
  const std::string& nameStr) const
{
  return smtk::dynamic_pointer_cast<const ModelEntityItem>(this->find(nameStr));
}

smtk::attribute::VoidItemPtr Attribute::findVoid(const std::string& nameStr)
{
  return smtk::dynamic_pointer_cast<VoidItem>(this->find(nameStr));
}
smtk::attribute::ConstVoidItemPtr Attribute::findVoid(const std::string& nameStr) const
{
  return smtk::dynamic_pointer_cast<const VoidItem>(this->find(nameStr));
}

smtk::attribute::MeshSelectionItemPtr Attribute::findMeshSelection(const std::string& nameStr)
{
  return smtk::dynamic_pointer_cast<MeshSelectionItem>(this->find(nameStr));
}
smtk::attribute::ConstMeshSelectionItemPtr Attribute::findMeshSelection(
  const std::string& nameStr) const
{
  return smtk::dynamic_pointer_cast<const MeshSelectionItem>(this->find(nameStr));
}

smtk::attribute::MeshItemPtr Attribute::findMesh(const std::string& nameStr)
{
  return smtk::dynamic_pointer_cast<MeshItem>(this->find(nameStr));
}
smtk::attribute::ConstMeshItemPtr Attribute::findMesh(const std::string& nameStr) const
{
  return smtk::dynamic_pointer_cast<const MeshItem>(this->find(nameStr));
}

smtk::attribute::DateTimeItemPtr Attribute::findDateTime(const std::string& nameStr)
{
  return smtk::dynamic_pointer_cast<DateTimeItem>(this->find(nameStr));
}
smtk::attribute::ConstDateTimeItemPtr Attribute::findDateTime(const std::string& nameStr) const
{
  return smtk::dynamic_pointer_cast<const DateTimeItem>(this->find(nameStr));
}

smtk::attribute::ReferenceItemPtr Attribute::findReference(const std::string& nameStr)
{
  return smtk::dynamic_pointer_cast<ReferenceItem>(this->find(nameStr));
}
smtk::attribute::ConstReferenceItemPtr Attribute::findReference(const std::string& nameStr) const
{
  return smtk::dynamic_pointer_cast<const ReferenceItem>(this->find(nameStr));
}

smtk::attribute::ResourceItemPtr Attribute::findResource(const std::string& nameStr)
{
  return smtk::dynamic_pointer_cast<ResourceItem>(this->find(nameStr));
}
smtk::attribute::ConstResourceItemPtr Attribute::findResource(const std::string& nameStr) const
{
  return smtk::dynamic_pointer_cast<const ResourceItem>(this->find(nameStr));
}

smtk::attribute::ComponentItemPtr Attribute::findComponent(const std::string& nameStr)
{
  return smtk::dynamic_pointer_cast<ComponentItem>(this->find(nameStr));
}
smtk::attribute::ConstComponentItemPtr Attribute::findComponent(const std::string& nameStr) const
{
  return smtk::dynamic_pointer_cast<const ComponentItem>(this->find(nameStr));
}
