//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================

#include "smtk/io/XmlDocV3Parser.h"
#define PUGIXML_HEADER_ONLY
#include "pugixml/src/pugixml.cpp"

#include "smtk/attribute/Attribute.h"
#include "smtk/attribute/ComponentItem.h"
#include "smtk/attribute/ComponentItemDefinition.h"
#include "smtk/attribute/DateTimeItem.h"
#include "smtk/attribute/DateTimeItemDefinition.h"
#include "smtk/attribute/Definition.h"
#include "smtk/attribute/ResourceItem.h"
#include "smtk/attribute/ResourceItemDefinition.h"

#include "smtk/common/DateTimeZonePair.h"
#include "smtk/common/StringUtil.h"

#include "smtk/model/Entity.h"
#include "smtk/model/EntityRef.h"

#include "smtk/resource/Component.h"
#include "smtk/resource/ComponentLinks.h"
#include "smtk/resource/Lock.h"
#include "smtk/resource/Resource.h"
#include "smtk/resource/ResourceLinks.h"

using namespace pugi;
using namespace smtk::io;
using namespace smtk;

XmlDocV3Parser::XmlDocV3Parser(smtk::attribute::ResourcePtr myResource)
  : XmlDocV2Parser(myResource)
{
}

XmlDocV3Parser::~XmlDocV3Parser()
{
}

bool XmlDocV3Parser::canParse(xml_document& doc)
{
  // Get the attribute resource node
  xml_node amnode = doc.child("SMTK_AttributeResource");
  if (amnode.empty())
  {
    return false;
  }

  pugi::xml_attribute xatt = amnode.attribute("Version");
  if (!xatt)
  {
    return false;
  }

  int versionNum = xatt.as_int();
  if (versionNum != 3)
  {
    return false;
  }

  return true;
}

bool XmlDocV3Parser::canParse(xml_node& node)
{
  // Check the name of the node
  std::string name = node.name();
  if (name != "SMTK_AttributeResource")
  {
    return false;
  }

  pugi::xml_attribute xatt = node.attribute("Version");
  if (!xatt)
  {
    return false;
  }

  int versionNum = xatt.as_int();
  if (versionNum != 3)
  {
    return false;
  }

  return true;
}

xml_node XmlDocV3Parser::getRootNode(xml_document& doc)
{
  xml_node amnode = doc.child("SMTK_AttributeResource");
  return amnode;
}

void XmlDocV3Parser::process(xml_document& doc)
{
  // Get the attribute resource node
  xml_node amnode = doc.child("SMTK_AttributeResource");

  // Check that there is content
  if (amnode.empty())
  {
    smtkWarningMacro(m_logger, "Missing SMTK_AttributeResource element");
    return;
  }

  this->process(amnode);
}

void XmlDocV3Parser::processDefinition(xml_node& defNode, smtk::attribute::DefinitionPtr def)
{
  // we just need to process Tags added in V3
  this->XmlDocV2Parser::processDefinition(defNode, def);
  xml_node tagsNode = defNode.child("Tags");
  if (tagsNode)
  {
    for (xml_node tagNode = tagsNode.child("Tag"); tagNode; tagNode = tagNode.next_sibling("Tag"))
    {
      xml_attribute name_att = tagNode.attribute("Name");
      std::string values = tagNode.text().get();
      if (values.empty())
      {
        bool success = def->addTag(smtk::attribute::Tag(name_att.value()));
        if (!success)
        {
          smtkWarningMacro(m_logger, "Could not add tag \"" << name_att.value() << "\"");
        }
      }
      else
      {
        xml_attribute sep_att = tagNode.attribute("Sep");
        std::string sep = sep_att ? sep_att.value() : ",";
        std::vector<std::string> vals = smtk::common::StringUtil::split(values, sep, false, false);
        bool success = def->addTag(
          smtk::attribute::Tag(name_att.value(), std::set<std::string>(vals.begin(), vals.end())));
        if (!success)
        {
          smtkWarningMacro(m_logger, "Could not add tag \"" << name_att.value() << "\"");
        }
      }
    }
  }
}

void XmlDocV3Parser::processAssociationDef(xml_node& node, smtk::attribute::DefinitionPtr def)
{
  std::string assocName = node.attribute("Name").value();
  if (assocName.empty())
  {
    assocName = def->type() + "Associations";
  }
  smtk::attribute::ReferenceItemDefinitionPtr assocDef =
    smtk::dynamic_pointer_cast<smtk::attribute::ReferenceItemDefinition>(
      smtk::attribute::ReferenceItemDefinition::New(assocName));
  this->processReferenceDef(node, assocDef);
  // We don't want reference items to handle "MembershipMask" but we do need
  // to support AssociationsDef entries with a MembershipMask. So add that here:
  xml_node mmask = node.child("MembershipMask");
  if (mmask)
  {
    assocDef->setAcceptsEntries("smtk::model::Resource", mmask.text().as_string(), true);
  }
  def->setLocalAssociationRule(assocDef);
}

void XmlDocV3Parser::processDateTimeDef(
  pugi::xml_node& node, attribute::DateTimeItemDefinitionPtr idef)
{
  // Process the common value item def stuff
  this->processItemDef(node, idef);

  xml_attribute xatt;
  xatt = node.attribute("NumberOfRequiredValues");
  std::size_t numberOfComponents = idef->numberOfRequiredValues();
  if (xatt)
  {
    numberOfComponents = xatt.as_uint();
    idef->setNumberOfRequiredValues(numberOfComponents);
  }

  xatt = node.attribute("DisplayFormat");
  if (xatt)
  {
    idef->setDisplayFormat(xatt.value());
  }

  xatt = node.attribute("ShowTimeZone");
  if (xatt)
  {
    idef->setUseTimeZone(xatt.as_bool());
  }

  xatt = node.attribute("ShowCalendarPopup");
  if (xatt)
  {
    idef->setEnableCalendarPopup(xatt.as_bool());
  }

  xml_node dnode = node.child("DefaultValue");
  if (dnode)
  {
    ::smtk::common::DateTimeZonePair dtz;
    std::string content = dnode.text().get();
    dtz.deserialize(content);
    idef->setDefaultValue(dtz);
  }
}

void XmlDocV3Parser::processDateTimeItem(pugi::xml_node& node, attribute::DateTimeItemPtr item)
{
  xml_attribute natt = node.attribute("NumberOfValues");
  if (!natt)
  {
    // Single value
    item->setNumberOfValues(1);
    xml_node noVal = node.child("UnsetVal");
    if (!noVal)
    {
      ::smtk::common::DateTimeZonePair dtz;
      std::string content = node.text().get();
      dtz.deserialize(content);
      item->setValue(dtz);
    }
  }
  else
  {
    // Multiple values
    std::size_t n = natt.as_uint();
    item->setNumberOfValues(n);
    xml_node valsNode = node.child("Values");
    if (valsNode)
    {
      for (xml_node val = valsNode.first_child(); val; val = val.next_sibling())
      {
        std::string nodeName = val.name();
        if (nodeName == "UnsetVal")
        {
          continue;
        }
        xml_attribute ixatt = val.attribute("Ith");
        if (!ixatt)
        {
          smtkErrorMacro(m_logger, "XML Attribute Ith is missing for Item: " << item->name());
          continue;
        }
        unsigned int i = ixatt.as_uint();
        if (i >= n)
        {
          smtkErrorMacro(
            m_logger, "XML Attribute Ith = " << i << " is out of range for Item: " << item->name());
          continue;
        }
        if (nodeName == "Val")
        {
          ::smtk::common::DateTimeZonePair dtz;
          std::string content = val.text().get();
          dtz.deserialize(content);
          item->setValue(static_cast<int>(i), dtz);
        }
        else
        {
          smtkErrorMacro(m_logger, "Unsupported Value Node Type  Item: " << item->name());
        } // else
      }   // for (val)
    }     // if (valsNode)
  }       // else
}

void XmlDocV3Parser::processReferenceItem(
  pugi::xml_node& node, smtk::attribute::ReferenceItemPtr item)
{
  xml_attribute xatt;
  xml_node valsNode;
  std::size_t i, n = item->numberOfValues();
  smtk::common::UUID uid;
  xml_node val;
  std::size_t numRequiredVals = item->numberOfRequiredValues();
  std::string attName;
  AttRefInfo info;
  if (!numRequiredVals || item->isExtensible())
  {
    // The node should have an attribute indicating how many values are
    // associated with the item
    xatt = node.attribute("NumberOfValues");
    if (!xatt)
    {
      smtkErrorMacro(
        m_logger, "XML Attribute NumberOfValues is missing for Item: " << item->name());
      return;
    }
    n = xatt.as_uint();
    item->setNumberOfValues(n);
  }
  if (!n)
  {
    return;
  }

  valsNode = node.child("Values");
  if (valsNode)
  {
    for (val = valsNode.child("Val"); val; val = val.next_sibling("Val"))
    {
      xatt = val.attribute("Ith");
      if (!xatt)
      {
        smtkErrorMacro(m_logger, "XML Attribute Ith is missing for Item: " << item->name());
        continue;
      }
      i = xatt.as_uint();
      if (i >= n)
      {
        smtkErrorMacro(
          m_logger, "XML Attribute Ith = " << i << " is out of range for Item: " << item->name());
        continue;
      }

      auto& links = item->attribute()->resource()->links().data();

      xml_node keyNode = val.child("Key");
      smtk::attribute::ReferenceItem::Key key(smtk::common::UUID(keyNode.child("_1_").text().get()),
        smtk::common::UUID(keyNode.child("_2_").text().get()));
      item->setObjectKey(static_cast<int>(i), key);

      xml_node rhsNode = val.child("RHS");
      smtk::common::UUID rhs1(rhsNode.child("_1_").text().get());
      smtk::common::UUID rhs2(rhsNode.child("_2_").text().get());

      xatt = val.attribute("Role");
      int role = xatt.as_int();

      xml_node surrogateNode = val.child("Surrogate");
      unsigned int surrogateIndex = surrogateNode.attribute("Index").as_uint();
      std::string surrogateTypeName(surrogateNode.attribute("TypeName").as_string());
      smtk::common::UUID surrogateId(surrogateNode.attribute("Id").as_string());
      std::string surrogateLocation(surrogateNode.attribute("Location").as_string());

      if (!links.has(key.first))
      {
        links.insert(smtk::resource::Surrogate(
                       surrogateIndex, surrogateTypeName, surrogateId, surrogateLocation),
          key.first, item->attribute()->resource()->id(), rhs1);
      }

      links.value(key.first).insert(key.second, item->attribute()->id(), rhs2, role);
    }
  }
  else if (numRequiredVals == 1)
  {
    val = node.child("Val");
    if (val)
    {
      auto& links = item->attribute()->resource()->links().data();

      xml_node keyNode = val.child("Key");
      smtk::attribute::ReferenceItem::Key key(smtk::common::UUID(keyNode.child("_1_").text().get()),
        smtk::common::UUID(keyNode.child("_2_").text().get()));
      item->setObjectKey(0, key);

      xml_node rhsNode = val.child("RHS");
      smtk::common::UUID rhs1(rhsNode.child("_1_").text().get());
      smtk::common::UUID rhs2(rhsNode.child("_2_").text().get());

      xatt = val.attribute("Role");
      int role = xatt.as_int();

      xml_node surrogateNode = val.child("Surrogate");
      unsigned int surrogateIndex = surrogateNode.attribute("Index").as_uint();
      std::string surrogateTypeName(surrogateNode.attribute("TypeName").as_string());
      smtk::common::UUID surrogateId(surrogateNode.attribute("Id").as_string());
      std::string surrogateLocation(surrogateNode.attribute("Location").as_string());

      if (!links.has(key.first))
      {
        links.insert(smtk::resource::Surrogate(
                       surrogateIndex, surrogateTypeName, surrogateId, surrogateLocation),
          key.first, item->attribute()->resource()->id(), rhs1);
      }

      links.value(key.first).insert(key.second, item->attribute()->id(), rhs2, role);
    }
  }
  else
  {
    smtkErrorMacro(m_logger, "XML Node Values is missing for Item: " << item->name());
  }
}

void XmlDocV3Parser::processReferenceDef(pugi::xml_node& node,
  smtk::attribute::ReferenceItemDefinitionPtr idef, const std::string& labelsElement)
{
  xml_node accepts, labels, child;
  xml_attribute xatt;
  int i;
  this->processItemDef(node, idef);

  accepts = node.child("Accepts");
  if (accepts)
  {
    std::string resourceName("Resource");
    for (child = accepts.first_child(); child; child = child.next_sibling())
    {
      if (child.name() == resourceName)
      {
        idef->setAcceptsEntries(
          child.attribute("Name").value(), child.attribute("Filter").value(), true);
      }
    }
  }

  xatt = node.attribute("LockType");
  if (xatt)
  {
    if (strcmp(xatt.as_string(), "DoNotLock") == 0)
    {
      idef->setLockType(smtk::resource::LockType::DoNotLock);
    }
    else if (strcmp(xatt.as_string(), "Read") == 0)
    {
      idef->setLockType(smtk::resource::LockType::Read);
    }
    else
    {
      idef->setLockType(smtk::resource::LockType::Write);
    }
  }

  xatt = node.attribute("NumberOfRequiredValues");
  if (xatt)
  {
    idef->setNumberOfRequiredValues(xatt.as_int());
  }

  xatt = node.attribute("Extensible");
  if (xatt)
  {
    idef->setIsExtensible(xatt.as_bool());
    xatt = node.attribute("MaxNumberOfValues");
    if (xatt)
    {
      idef->setMaxNumberOfValues(xatt.as_uint());
    }
  }

  // Lets see if there are labels
  if (node.child("Labels"))
  {
    smtkErrorMacro(
      m_logger, "Labels has been changed to " << labelsElement << " : " << idef->name());
  }
  labels = node.child(labelsElement.c_str());
  if (labels)
  {
    // Are we using a common label?
    xatt = labels.attribute("CommonLabel");
    if (xatt)
    {
      idef->setCommonValueLabel(xatt.value());
    }
    else
    {
      for (child = labels.first_child(), i = 0; child; child = child.next_sibling(), i++)
      {
        idef->setValueLabel(i, child.value());
      }
    }
  }
}

void XmlDocV3Parser::processResourceItem(
  pugi::xml_node& node, smtk::attribute::ResourceItemPtr item)
{
  this->processReferenceItem(node, item);
}

void XmlDocV3Parser::processResourceDef(
  pugi::xml_node& node, smtk::attribute::ResourceItemDefinitionPtr idef)
{
  this->processReferenceDef(node, idef, "ResourceLabels");
}

void XmlDocV3Parser::processComponentItem(
  pugi::xml_node& node, smtk::attribute::ComponentItemPtr item)
{
  this->processReferenceItem(node, item);
}

void XmlDocV3Parser::processComponentDef(
  pugi::xml_node& node, smtk::attribute::ComponentItemDefinitionPtr idef)
{
  this->processReferenceDef(node, idef, "ComponentLabels");
}
