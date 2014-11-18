#include "implement_an_operator.h"

#include "smtk/AutoInit.h"

#include "smtk/common/UUID.h"

#include "smtk/attribute/Attribute.h"
#include "smtk/attribute/Definition.h"
#include "smtk/attribute/IntItem.h"
#include "smtk/attribute/ModelEntityItem.h"

#include "smtk/model/Bridge.h"
#include "smtk/model/DefaultBridge.h"
#include "smtk/model/GroupEntity.h"
#include "smtk/model/Manager.h"
#include "smtk/model/ModelEntity.h"
#include "smtk/model/Volume.h"

#include "smtk/common/testing/cxx/helpers.h"
#include "smtk/model/testing/cxx/helpers.h"

#include "smtk/Options.h"

// ++ 1 ++
// Include the encoded XML describing the operator class.
// This is generated by CMake.
#include "implement_an_operator_xml.h"
// -- 1 --

using namespace smtk::common;
using namespace smtk::model;
using smtk::attribute::IntItem;

namespace ex {

// ++ 2 ++
OperatorResult CounterOperator::operateInternal()
{
  // Get the attribute holding parameter values:
  OperatorSpecification params = this->specification();

  // Get the input model to be processed:
  ModelEntity model =
    params->findModelEntity("model")->value();

  // Decide whether we should count cells or groups
  // of the model:
  int countGroups =
    params->findInt("count groups")->value();

  // Create the attribute holding the results of
  // our operation using a convenience method
  // provided by the Operator base class.
  // Our operation is simple; we always succeed.
  OperatorResult result =
    this->createResult(OPERATION_SUCCEEDED);

  // Fetch the item to store our output:
  smtk::attribute::IntItemPtr cellCount =
    result->findInt("count");

  cellCount->setValue(countGroups ?
    model.groups().size() :
    model.cells().size());

  return result;
}
// -- 2 --

} // namespace ex

// ++ 3 ++
// Implement methods from smtkDeclareModelOperator()
// and provide an auto-init object for registering the
// operator with the bridge.
smtkImplementsModelOperator(
  ex::CounterOperator, // The class name (include all namespaces)
  ex_counter,          // The "component" name (for auto-init)
  "counter",           // The user-printable operator name.
  implement_an_operator_xml, // An XML description (or NULL).
  smtk::model::DefaultBridge); // The modeling kernel this operator uses.
// -- 3 --

void testOperator(ModelEntity model)
{
  // Get the default bridge for our model manager:
  smtk::model::BridgePtr bridge =
    model.manager()->bridgeForModel(UUID::null());

  // Ask the bridge to create an operator:
  ex::CounterOperator::Ptr op =
    smtk::dynamic_pointer_cast<ex::CounterOperator>(
      bridge->op("counter"));

  op->ensureSpecification();
  smtk::attribute::ModelEntityItemPtr input =
    op->specification()->findModelEntity("model");
  input->setValue(model);

  test(!!op, "Could not create operator.");
  test(
    op->operate()->findInt("count")->value() == 1,
    "Did not return the proper number of top-level cells.");

  op->specification()->findInt("count groups")->setValue(1);
  test(
    op->operate()->findInt("count")->value() == 0,
    "Did not return the proper number of top-level group.");
}

int main()
{
  int status = 0;

  Manager::Ptr manager = Manager::create();
  UUIDArray uids = smtk::model::testing::createTet(manager);

  ModelEntity model = manager->addModel(3, 3, "TestModel");
  Volume tet = Volume(manager, uids[21]);
  model.addCell(tet);

  try {

    testOperator(model);

  } catch (const std::string& msg) {
    (void) msg; // Ignore the message; it's already been printed.
    std::cerr << "Exiting...\n";
    status = -1;
  }

  return status;
}
