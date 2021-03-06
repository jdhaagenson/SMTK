//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================
#include "smtk/session/mesh/operators/Export.h"

#include "smtk/session/mesh/Resource.h"
#include "smtk/session/mesh/Session.h"

#include "smtk/attribute/Attribute.h"
#include "smtk/attribute/FileItem.h"
#include "smtk/attribute/StringItem.h"

#include "smtk/io/WriteMesh.h"

#include "smtk/model/Group.h"
#include "smtk/model/Model.h"

#include "smtk/session/mesh/Export_xml.h"

using namespace smtk::model;
using namespace smtk::common;

namespace smtk
{
namespace session
{
namespace mesh
{

void breakMaterialsByAssociation(const smtk::mesh::ResourcePtr& c)
{
  //for each association we iterate the meshsets
  smtk::model::EntityRefArray refs;
  c->meshes().modelEntities(refs);

  int domain = 0;
  for (auto&& ref : refs)
  {
    smtk::mesh::MeshSet m = c->findAssociatedMeshes(ref);
    c->setDomainOnMeshes(m, smtk::mesh::Domain(domain++));
  }
}

smtk::session::mesh::Export::Result Export::operateInternal()
{
  smtk::attribute::FileItem::Ptr filePathItem = this->parameters()->findFile("filename");

  std::string filePath = filePathItem->value();

  smtk::model::Models datasets = this->parameters()->associatedModelEntities<smtk::model::Models>();
  if (datasets.empty())
  {
    smtkErrorMacro(this->log(), "No models to save.");
    return this->createResult(smtk::operation::Operation::Outcome::FAILED);
  }

  smtk::model::Model dataset = datasets[0];

  smtk::session::mesh::Resource::Ptr resource =
    std::static_pointer_cast<smtk::session::mesh::Resource>(dataset.component()->resource());
  smtk::session::mesh::Session::Ptr session = resource->session();

  smtk::mesh::ResourcePtr meshResource = resource->resource();

  if (meshResource == nullptr || !meshResource->isValid())
  {
    smtkErrorMacro(this->log(), "No mesh resource associated with this model.");
    return this->createResult(smtk::operation::Operation::Outcome::FAILED);
  }

  breakMaterialsByAssociation(meshResource);

  bool writeSuccess = smtk::io::writeMesh(filePath, meshResource);

  if (!writeSuccess)
  {
    smtkErrorMacro(this->log(), "Mesh resource failed to write.");
    return this->createResult(smtk::operation::Operation::Outcome::FAILED);
  }

  Result result = this->createResult(smtk::operation::Operation::Outcome::SUCCEEDED);
  return result;
}

const char* Export::xmlDescription() const
{
  return Export_xml;
}

} // namespace mesh
} //namespace session
} // namespace smtk
