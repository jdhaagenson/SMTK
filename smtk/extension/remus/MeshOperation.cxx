//=============================================================================
//
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//
//=============================================================================
#include "smtk/extension/remus/MeshOperation.h"

#include <remus/client/Client.h>
#include <remus/proto/SMTKMeshSubmission.h>

#include "smtk/model/CellEntity.h"
#include "smtk/model/Model.h"
#include "smtk/model/Resource.h"
#include "smtk/model/Session.h"

#include "smtk/mesh/core/Resource.h"

#include "smtk/attribute/Attribute.h"
#include "smtk/attribute/ComponentItem.h"
#include "smtk/attribute/ReferenceItem.h"
#include "smtk/attribute/StringItem.h"

#include "smtk/io/SaveJSON.h"
#include "smtk/io/SaveJSON.txx"

//todo: remove this once remus supports automatic transfer of FileHandles
// and Destructive Read of FileHandles
//force to use filesystem version 3
#define BOOST_FILESYSTEM_VERSION 3
#include <boost/filesystem.hpp>

//todo: remove this once remus Issue #183 has been resolved.
// #include <boost/date_time/posix_time/posix_time.hpp>
// #include <boost/thread/thread.hpp>

#include "smtk/extension/remus/MeshOperation_xml.h"

using namespace smtk::model;

namespace
{

std::string extractModelUUIDSAsJSON(smtk::model::Models const& models)
{
  typedef smtk::model::Models::const_iterator model_const_it;
  smtk::common::UUIDArray modelIds;
  for (model_const_it i = models.begin(); i != models.end(); ++i)
  {
    modelIds.push_back(i->entity());
    //do we need to export submodels?
  }

  cJSON* top = cJSON_CreateObject();
  cJSON_AddItemToObject(top, "ids", smtk::io::SaveJSON::createUUIDArray(modelIds));
  char* json = cJSON_Print(top);
  std::string uuidsSerialized(json);
  free(json);
  cJSON_Delete(top);
  return uuidsSerialized;
}
}

namespace smtk
{
namespace extension
{
namespace remus
{

MeshOperation::MeshOperation()
{
}

bool MeshOperation::ableToOperate()
{
  return this->Superclass::ableToOperate() &&
    smtk::model::Model(this->parameters()->associations()->valueAs<smtk::model::Entity>())
      .isValid();
  // TODO: Add tests to verify that model dimension matches job requirements.
}

MeshOperation::Result MeshOperation::operateInternal()
{
  auto models = this->parameters()->associatedModelEntities<smtk::model::Models>();
  smtk::attribute::StringItemPtr endpointItem = this->parameters()->findString("endpoint");
  smtk::attribute::StringItemPtr requirementsItem =
    this->parameters()->findString("remusRequirements");
  smtk::attribute::StringItemPtr attributeItem =
    this->parameters()->findString("meshingControlAttributes");

  //warn if the models to mesh are empty
  {
    bool modelsAreEmpty = true;
    for (auto& model : models)
    {
      if (!model.cells().empty())
      {
        modelsAreEmpty = false;
      }
    }
    if (modelsAreEmpty)
    {
      smtkWarningMacro(this->log(), "Model contains no cells.");
    }
  }

  //convert the model and uuids to mesh into a string representation
  smtk::model::Resource::Ptr resource =
    std::dynamic_pointer_cast<smtk::model::Resource>(models[0].component()->resource());

  std::string modelSerialized = smtk::io::SaveJSON::fromModelResource(resource);
  std::string modelUUIDSSerialized = extractModelUUIDSAsJSON(models);

  //deserialize the reqs from the string
  std::istringstream buffer(requirementsItem->value());
  ::remus::proto::JobRequirements reqs;
  buffer >> reqs;

  ::remus::proto::SMTKMeshSubmission submission(reqs);
  submission.model(modelSerialized, ::remus::common::ContentFormat::JSON);
  submission.attributes(attributeItem->value(), ::remus::common::ContentFormat::XML);
  submission.modelItemsToMesh(modelUUIDSSerialized, ::remus::common::ContentFormat::JSON);

  //now that we have the submission, construct a remus client to submit it
  const ::remus::client::ServerConnection conn =
    ::remus::client::make_ServerConnection(endpointItem->value());

  ::remus::client::Client client(conn);

  //the worker could have gone away while this operator was invoked, or maybe somebody submitted
  //the job using stale data from an older server

  smtkInfoMacro(this->log(), "[remus] Asking Server if it supports job reqs");
  ::remus::proto::Job job = ::remus::proto::make_invalidJob();
  if (client.canMesh(reqs))
  {
    job = client.submitJob(submission);
    smtkInfoMacro(
      this->log(), "[remus] Submitted Job to Server: " << ::remus::proto::to_string(job));
  }

  //once the job is submitted, we wait for the results to come back
  bool haveResultFromWorker = false;
  if (job.valid())
  {
    smtkInfoMacro(this->log(), "[remus] Querying Status of: " << ::remus::proto::to_string(job));
    ::remus::proto::JobStatus currentWorkerStatus = client.jobStatus(job);
    ::remus::proto::JobProgress lastProgress;
    while (currentWorkerStatus.good())
    { //we need this to not be a busy wait
      //for now lets call sleep to make this less 'heavy'
      // boost::this_thread::sleep( boost::posix_time::milliseconds(250) );
      currentWorkerStatus = client.jobStatus(job);
      // Lets see if the progress has changed
      if (currentWorkerStatus.progress() != lastProgress)
      {
        lastProgress = currentWorkerStatus.progress();
        smtkInfoMacro(this->log(), lastProgress.message());
      }
    }
    smtkInfoMacro(this->log(),
      "[remus] Final Status of: " << ::remus::proto::to_string(job)
                                  << "is: " << ::remus::to_string(currentWorkerStatus.status()));
    haveResultFromWorker = currentWorkerStatus.finished();
  }

  //the results are the model which we accept
  Result result =
    this->createResult(haveResultFromWorker ? smtk::operation::Operation::Outcome::SUCCEEDED
                                            : smtk::operation::Operation::Outcome::FAILED);
  if (haveResultFromWorker)
  {
    //now fetch the latest results from the server
    ::remus::proto::JobResult meshMetaData = client.retrieveResults(job);

    smtk::mesh::ManagerPtr meshManager = resource->meshes();

    //determine all existing mesh resource
    typedef std::map<smtk::common::UUID, smtk::mesh::ResourcePtr> MeshResourceStorage;
    MeshResourceStorage existingCollections(
      meshManager->meshResourceBegin(), meshManager->meshResourceEnd());

    //parse the job result as json mesh data
    cJSON* root = cJSON_Parse(meshMetaData.data());
    smtk::io::LoadJSON::ofMeshesOfModel(root, resource);
    cJSON_Delete(root);

    //
    //iterate over all mesh resources looking for new mesh resources. When we find
    //a new mesh mesh resource, we will delete the file that was used to generate
    //that mesh resource, as that file is meant to be temporary and only exist
    //for data transfer back from the worker.
    //
    //This all should be removed, and instead remus should handle all this logic
    //
    //
    //
    for (smtk::mesh::Manager::const_iterator i = meshManager->meshResourceBegin();
         i != meshManager->meshResourceEnd(); ++i)
    {
      smtk::mesh::ResourcePtr meshResource = i->second;
      smtk::common::UUID meshResourceUUID = i->first;
      if (existingCollections.find(meshResourceUUID) == existingCollections.end())
      { //found a new mesh resource
        std::string location = meshResource->readLocation().absolutePath();
        if (!location.empty())
        { //delete the file if it exists
          ::boost::filesystem::path cpath(location);
          ::boost::filesystem::remove(cpath);
        }

        //clear the read write locations so it looks like this
        //mesh was created from being in memory
        meshResource->clearReadWriteLocations();

        //fetch the allocator so that the mesh resource modified bit becomes
        //dirty. This is needed so it looks like this mesh was created in-memory
        //and not loaded from file.
        meshResource->interface()->allocator();
      }
    }

    //mark all models and submodels as modified
    smtk::model::Models allModels = models;
    for (smtk::model::Models::const_iterator m = models.begin(); m != models.end(); ++m)
    {
      smtk::model::Models submodels = m->submodels();
      allModels.insert(allModels.end(), submodels.begin(), submodels.end());
    }
    smtk::attribute::ComponentItem::Ptr modified = result->findComponent("modified");
    for (auto model : allModels)
    {
      modified->setValue(model.component());
    }

    result->findComponent("mesh_created")
      ->setValuesVia(models.begin(), models.end(),
        [](const smtk::model::Model& model) { return model.component(); });
  }
  return result;
}

const char* MeshOperation::xmlDescription() const
{
  return MeshOperation_xml;
}
}
}
}
