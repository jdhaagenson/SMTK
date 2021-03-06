//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================

#include <algorithm>

#include "smtk/attribute/Definition.h"
#include "smtk/attribute/RefItem.h"
#include "smtk/attribute/RefItemDefinition.h"
#include "smtk/attribute/Resource.h"
#include "smtk/attribute/ValueItem.h"
#include "smtk/attribute/ValueItemDefinition.h"

#include <iostream>

using namespace smtk::attribute;

ValueItemDefinition::ValueItemDefinition(const std::string& myName)
  : ItemDefinition(myName)
{
  m_defaultDiscreteIndex = -1;
  m_hasDefault = false;
  m_useCommonLabel = false;
  m_numberOfRequiredValues = 1;
  m_maxNumberOfValues = 0;
  m_isExtensible = false;
  m_expressionDefinition = RefItemDefinition::New("expression");
  m_expressionDefinition->setNumberOfRequiredValues(1);
}

ValueItemDefinition::~ValueItemDefinition()
{
}

bool ValueItemDefinition::setNumberOfRequiredValues(std::size_t esize)
{
  if (esize == m_numberOfRequiredValues)
  {
    return true;
  }
  if (m_maxNumberOfValues && (esize > m_maxNumberOfValues))
  {
    return false;
  }

  m_numberOfRequiredValues = esize;
  if (!this->hasValueLabels())
  {
    return true;
  }
  if (!(m_useCommonLabel || m_isExtensible))
  {
    m_valueLabels.resize(esize);
  }
  return true;
}

bool ValueItemDefinition::setMaxNumberOfValues(std::size_t esize)
{
  if (esize && (esize < m_numberOfRequiredValues))
  {
    return false;
  }
  m_maxNumberOfValues = esize;
  return true;
}

void ValueItemDefinition::setValueLabel(std::size_t element, const std::string& elabel)
{
  if (m_isExtensible)
  {
    return;
  }
  if (m_valueLabels.size() != m_numberOfRequiredValues)
  {
    m_valueLabels.resize(m_numberOfRequiredValues);
  }
  m_useCommonLabel = false;
  assert(m_valueLabels.size() > element);
  m_valueLabels[element] = elabel;
}

void ValueItemDefinition::setCommonValueLabel(const std::string& elabel)
{
  if (m_valueLabels.size() != 1)
  {
    m_valueLabels.resize(1);
  }
  m_useCommonLabel = true;
  assert(!m_valueLabels.empty());
  m_valueLabels[0] = elabel;
}

std::string ValueItemDefinition::valueLabel(std::size_t element) const
{
  if (m_useCommonLabel)
  {
    assert(!m_valueLabels.empty());
    return m_valueLabels[0];
  }
  if (element < m_valueLabels.size())
  {
    return m_valueLabels[element];
  }
  return ""; // If we threw execeptions this method could return const string &
}

bool ValueItemDefinition::isValidExpression(smtk::attribute::AttributePtr exp) const
{
  if (m_expressionDefinition->attributeDefinition() && m_expressionDefinition->isValueValid(exp))
  {
    return true;
  }
  return false;
}

bool ValueItemDefinition::allowsExpressions() const
{
  return m_expressionDefinition->attributeDefinition() ? true : false;
}

smtk::attribute::DefinitionPtr ValueItemDefinition::expressionDefinition() const
{
  return m_expressionDefinition->attributeDefinition();
}

void ValueItemDefinition::setExpressionDefinition(smtk::attribute::DefinitionPtr exp)
{
  m_expressionDefinition->setAttributeDefinition(exp);
}

void ValueItemDefinition::buildExpressionItem(ValueItem* vitem, int position) const
{
  smtk::attribute::RefItemPtr aref = smtk::dynamic_pointer_cast<smtk::attribute::RefItem>(
    m_expressionDefinition->buildItem(vitem, position, -1));
  aref->setDefinition(m_expressionDefinition);
  assert(vitem->m_expressions.size() > static_cast<size_t>(position));
  vitem->m_expressions[static_cast<size_t>(position)] = aref;
}

void ValueItemDefinition::buildChildrenItems(ValueItem* vitem) const
{
  std::map<std::string, smtk::attribute::ItemDefinitionPtr>::const_iterator it;
  smtk::attribute::ItemPtr child;
  for (it = m_itemDefs.begin(); it != m_itemDefs.end(); it++)
  {
    child = it->second->buildItem(vitem, 0, -1);
    child->setDefinition(it->second);
    vitem->m_childrenItems[it->first] = child;
  }
}

void ValueItemDefinition::setDefaultDiscreteIndex(int discreteIndex)
{
  m_defaultDiscreteIndex = discreteIndex;
  this->updateDiscreteValue();
  m_hasDefault = true;
}

bool ValueItemDefinition::addChildItemDefinition(smtk::attribute::ItemDefinitionPtr cdef)
{
  // First see if there is a item by the same name
  if (this->hasChildItemDefinition(cdef->name()))
  {
    return false;
  }
  m_itemDefs[cdef->name()] = cdef;
  return true;
}

bool ValueItemDefinition::addConditionalItem(
  const std::string& valueName, const std::string& itemName)
{
  // Do we have this valueName?
  if (std::find(m_discreteValueEnums.begin(), m_discreteValueEnums.end(), valueName) ==
    m_discreteValueEnums.end())
  {
    return false;
  }
  // Next do we have such an iten definition?
  if (!this->hasChildItemDefinition(itemName))
  {
    return false;
  }

  // Finally we need to verify that we don't already have this item assigned
  if (this->hasChildItemDefinition(valueName, itemName))
  {
    return false;
  }

  // create the association
  m_valueToItemAssociations[valueName].push_back(itemName);
  m_itemToValueAssociations[itemName].insert(valueName);
  return true;
}

std::vector<std::string> ValueItemDefinition::conditionalItems(const std::string& valueName) const
{
  // Do we have this valueName?
  if (std::find(m_discreteValueEnums.begin(), m_discreteValueEnums.end(), valueName) ==
    m_discreteValueEnums.end())
  {
    std::vector<std::string> temp;
    return temp;
  }
  std::map<std::string, std::vector<std::string> >::const_iterator citer =
    m_valueToItemAssociations.find(valueName);
  // Does the value have conditional items associated with it?
  if (citer == m_valueToItemAssociations.end())
  {
    std::vector<std::string> dummy;
    return dummy;
  }
  return citer->second;
}

void ValueItemDefinition::updateCategories()
{
  for (auto& i : m_itemDefs)
  {
    i.second->updateCategories();
  }
}

void ValueItemDefinition::setIsExtensible(bool mode)
{
  m_isExtensible = mode;
  if (!this->hasValueLabels())
  {
    // If there are no value labels there is nothing to do
    return;
  }

  if (mode && !this->usingCommonLabel())
  {
    // Need to clear individual labels - can only use common label with
    // extensible values
    this->setCommonValueLabel("");
  }
}

void ValueItemDefinition::copyTo(
  ValueItemDefinitionPtr def, smtk::attribute::ItemDefinition::CopyInfo& info) const
{
  std::size_t i;

  ItemDefinition::copyTo(def);

  if (m_units != "")
  {
    def->setUnits(m_units);
  }

  if (this->allowsExpressions())
  {
    // Set expression definition (if possible)
    std::string typeStr = this->expressionDefinition()->type();
    smtk::attribute::DefinitionPtr exp = info.ToResource.findDefinition(typeStr);
    if (exp)
    {
      def->setExpressionDefinition(exp);
    }
    else
    {
      std::cout << "Adding definition \"" << typeStr << "\" to copy-expression queue" << std::endl;

      info.UnresolvedExpItems.push(std::make_pair(typeStr, def));
    }
  }

  def->setNumberOfRequiredValues(m_numberOfRequiredValues);
  def->setMaxNumberOfValues(m_maxNumberOfValues);
  def->setIsExtensible(m_isExtensible);

  // Add label(s)
  if (m_useCommonLabel)
  {
    assert(!m_valueLabels.empty());
    def->setCommonValueLabel(m_valueLabels[0]);
  }
  else if (this->hasValueLabels())
  {
    for (i = 0; i < m_valueLabels.size(); ++i)
    {
      def->setValueLabel(i, m_valueLabels[i]);
    }
  }

  // Add children item definitions
  if (m_itemDefs.size() > 0)
  {
    std::map<std::string, smtk::attribute::ItemDefinitionPtr>::const_iterator itemDefMapIter =
      m_itemDefs.begin();
    for (; itemDefMapIter != m_itemDefs.end(); itemDefMapIter++)
    {
      smtk::attribute::ItemDefinitionPtr itemDef = itemDefMapIter->second->createCopy(info);
      def->addChildItemDefinition(itemDef);
    }
  }

  // Add condition items
  if (m_valueToItemAssociations.size() > 0)
  {
    std::map<std::string, std::vector<std::string> >::const_iterator mapIter =
      m_valueToItemAssociations.begin();
    std::string value;
    std::vector<std::string>::const_iterator itemIter;
    for (; mapIter != m_valueToItemAssociations.end(); mapIter++)
    {
      value = mapIter->first;
      itemIter = mapIter->second.begin();
      for (; itemIter != mapIter->second.end(); itemIter++)
      {
        def->addConditionalItem(value, *itemIter);
      }
    }
  }
}

bool ValueItemDefinition::getEnumIndex(const std::string& enumVal, std::size_t& index) const
{
  std::size_t i, n = m_discreteValueEnums.size();
  for (i = static_cast<std::size_t>(0); i < n; i++)
  {
    if (m_discreteValueEnums.at(i) == enumVal)
    {
      index = i;
      return true;
    }
  }
  return false;
}
