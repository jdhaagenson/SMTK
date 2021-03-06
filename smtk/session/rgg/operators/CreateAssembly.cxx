//=============================================================================
// Copyright (c) Kitware, Inc.
// All rights reserved.
// See LICENSE.txt for details.
//
// This software is distributed WITHOUT ANY WARRANTY; without even
// the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE.  See the above copyright notice for more information.
//=============================================================================
#include "smtk/session/rgg/operators/CreateAssembly.h"

#include "smtk/session/rgg/Session.h"

#include "smtk/attribute/Attribute.h"
#include "smtk/attribute/ComponentItem.h"
#include "smtk/attribute/DoubleItem.h"
#include "smtk/attribute/GroupItem.h"
#include "smtk/attribute/IntItem.h"
#include "smtk/attribute/ModelEntityItem.h"
#include "smtk/attribute/StringItem.h"
#include "smtk/attribute/VoidItem.h"
#include "smtk/io/Logger.h"

#include "smtk/model/EntityRef.h"
#include "smtk/model/Group.h"
#include "smtk/model/Resource.h"

#include "smtk/session/rgg/CreateAssembly_xml.h"

using namespace smtk::model;

namespace smtk
{
namespace session
{
namespace rgg
{

CreateAssembly::Result CreateAssembly::operateInternal()
{
  Result result = this->createResult(smtk::operation::Operation::Outcome::FAILED);
  auto entities = this->parameters()->associatedModelEntities<smtk::model::EntityRefArray>();
  // Since we are in creating mode, it must be a model
  if (entities.empty() || !entities[0].isModel())
  {
    smtkErrorMacro(this->log(), "An invalid model is provided for create assembly op");
    return this->createResult(smtk::operation::Operation::Outcome::FAILED);
  }

  EntityRef parent = entities[0];
  smtk::model::Group assembly = parent.resource()->addGroup(0, "group"); // Assign the name later
  smtk::model::Model model = entities[0].as<smtk::model::Model>();
  model.addGroup(assembly);
  BitFlags mask(0);
  mask |= smtk::model::AUX_GEOM_ENTITY;
  mask |= smtk::model::INSTANCE_ENTITY;
  assembly.setMembershipMask(mask);

  // Populate the assembly after the model has been createed
  CreateAssembly::populateAssembly(this, assembly, true);
  // Update the latest assembly in the model
  parent.setStringProperty("latest assembly", assembly.entity().toString());

  result = this->createResult(smtk::operation::Operation::Outcome::SUCCEEDED);

  result->findComponent("modified")->appendValue(parent.component());
  result->findComponent("created")->appendValue(assembly.component());

  return result;
}

void CreateAssembly::populateAssembly(
  smtk::operation::Operation* op, smtk::model::Group& assembly, bool createMode)
{
  assembly.setStringProperty("rggType", SMTK_SESSION_RGG_ASSEMBLY);
  smtk::attribute::StringItemPtr nameItem = op->parameters()->findString("name");
  std::string assName;
  if (nameItem != nullptr && !nameItem->value(0).empty())
  {
    assName = nameItem->value(0);
    assembly.setName(nameItem->value(0));
  }

  smtk::attribute::StringItemPtr labelItem = op->parameters()->findString("label");
  if (labelItem != nullptr && !labelItem->value(0).empty())
  {
    smtk::model::Model model = assembly.owningModel();
    std::string labelValue = labelItem->value(0);
    if ((assembly.hasStringProperty("label") &&
          assembly.stringProperty("label")[0] != labelValue) ||
      createMode)
    { // Only generate label if needed
      if (model.hasStringProperty("assembly labels list"))
      {
        smtk::model::StringList& labels = model.stringProperty("assembly labels list");
        int count = 0;
        while (std::find(std::begin(labels), std::end(labels), labelValue) != std::end(labels))
        { // need to generate a new label
          labelValue = "A" + std::to_string(count);
          count++;
        }
        // Update the label list
        labels.push_back(labelValue);
      }
      else
      {
        model.setStringProperty("assembly labels list", labelValue);
      }
    }
    assembly.setStringProperty(labelItem->name(), labelValue);
  }

  smtk::attribute::DoubleItemPtr colorI = op->parameters()->findDouble("color");
  if (colorI)
  {
    smtk::model::FloatList color = smtk::model::FloatList{ colorI->begin(), colorI->end() };
    if (color.size() != 4 && (color.size() != 0))
    {
      smtkErrorMacro(op->log(), "Color item does not have 4 values");
    }
    else
    {
      assembly.setColor(color);
    }
  }

  // Pins and their layouts
  // Op would store all pin uuids as a string property on the assembly,
  // then for each pin op would store its layout as an int property.
  smtk::attribute::GroupItemPtr piecesGItem = op->parameters()->findGroup("pins and layouts");
  size_t numPins;
  if (piecesGItem != nullptr)
  {
    smtk::model::StringList pinIds;
    numPins = piecesGItem->numberOfGroups();
    for (std::size_t index = 0; index < numPins; index++)
    {
      smtk::attribute::StringItemPtr pinIdItem =
        piecesGItem->findAs<smtk::attribute::StringItem>(index, "pin UUID");
      smtk::attribute::IntItemPtr schemaPlanItem =
        piecesGItem->findAs<smtk::attribute::IntItem>(index, "schema plan");
      smtk::attribute::DoubleItemPtr coordsItem =
        piecesGItem->findAs<smtk::attribute::DoubleItem>(index, "coordinates");

      std::string uuid = pinIdItem->value();
      pinIds.push_back(uuid);
      smtk::model::IntegerList layout(schemaPlanItem->begin(), schemaPlanItem->end());
      assembly.setIntegerProperty(uuid, layout);
      smtk::model::FloatList coordinates(coordsItem->begin(), coordsItem->end());
      assembly.setFloatProperty(uuid, coordinates);
    }
    assembly.setStringProperty("pins", pinIds);
  }
  else
  {
    smtkErrorMacro(
      op->log(), "assembly " << assName << " does not have pins and their layouts info");
  }

  smtk::attribute::ModelEntityItemPtr ductIdItem =
    op->parameters()->findModelEntity("associated duct");
  if (ductIdItem && !createMode && (ductIdItem->numberOfValues() == 1))
  {
    assembly.setStringProperty("associated duct", ductIdItem->value().entity().toString());
  }

  smtk::attribute::VoidItemPtr isCenteredPinsItem = op->parameters()->findVoid("center pins");
  if (isCenteredPinsItem->isEnabled() || createMode)
  {
    assembly.setIntegerProperty(isCenteredPinsItem->name(), 1);
  }
  else
  {
    assembly.setIntegerProperty(isCenteredPinsItem->name(), 0);
  }

  smtk::attribute::DoubleItemPtr pitchesItem = op->parameters()->findDouble("pitches");
  if (pitchesItem)
  {
    if (pitchesItem->numberOfValues() == 1)
    { // Default value condition
      smtk::model::FloatList pitches{ pitchesItem->value(), pitchesItem->value() };
      assembly.setFloatProperty(pitchesItem->name(), pitches);
    }
    else if (pitchesItem->numberOfValues() == 2)
    {
      smtk::model::FloatList pitches(pitchesItem->begin(), pitchesItem->end());
      assembly.setFloatProperty(pitchesItem->name(), pitches);
    }
    else
    {
      smtkErrorMacro(op->log(), "assembly " << assName << " does not have valid pitch values");
    }
  }

  smtk::attribute::IntItemPtr latticeSizeItem = op->parameters()->findInt("lattice size");
  if (latticeSizeItem)
  {
    if (latticeSizeItem->numberOfValues() == 1)
    { // Default value condition
      if (!assembly.owningModel().hasIntegerProperty("hex"))
      {
        smtkErrorMacro(op->log(), "Core " << assembly.owningModel().name()
                                          << " does not have a geometry type value");
      }
      else
      {
        int unitSize = (assembly.owningModel().integerProperty("hex")[0]) ? 1 : 4;
        smtk::model::IntegerList size = { unitSize, unitSize };
        assembly.setIntegerProperty(latticeSizeItem->name(), size);
      }
    }
    else if (latticeSizeItem->numberOfValues() == 2 && !createMode)
    { // Edit assembly
      smtk::model::IntegerList size(latticeSizeItem->begin(), latticeSizeItem->end());
      assembly.setIntegerProperty(latticeSizeItem->name(), size);
    }
    else
    {
      smtkErrorMacro(op->log(), "assembly " << assName << " does not have a valid lattice size");
    }
  }

  smtk::attribute::IntItemPtr zAxisItem = op->parameters()->findInt("z axis");
  if (zAxisItem)
  {
    assembly.setIntegerProperty(zAxisItem->name(), zAxisItem->value());
    smtkWarningMacro(op->log(), "For now, SMTK assembly does not support z axis value yet");
  }
  else
  {
    smtkErrorMacro(op->log(), "assembly " << assName << " does not have a valid z axis value");
  }
}

const char* CreateAssembly::xmlDescription() const
{
  return CreateAssembly_xml;
}
} // namespace rgg
} //namespace session
} // namespace smtk
