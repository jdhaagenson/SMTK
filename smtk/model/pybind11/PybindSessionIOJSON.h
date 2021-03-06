//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================

#ifndef pybind_smtk_model_SessionIOJSON_h
#define pybind_smtk_model_SessionIOJSON_h

#include <pybind11/pybind11.h>

#include "smtk/model/SessionIOJSON.h"

#include "smtk/model/Session.h"
#include "smtk/model/SessionIO.h"

namespace py = pybind11;

py::class_< smtk::model::SessionIOJSON, smtk::model::SessionIO > pybind11_init_smtk_model_SessionIOJSON(py::module &m)
{
  py::class_< smtk::model::SessionIOJSON, smtk::model::SessionIO > instance(m, "SessionIOJSON");
  instance
    .def(py::init<>())
    .def(py::init<::smtk::model::SessionIOJSON const &>())
    .def("deepcopy", (smtk::model::SessionIOJSON & (smtk::model::SessionIOJSON::*)(::smtk::model::SessionIOJSON const &)) &smtk::model::SessionIOJSON::operator=)
    .def_static("loadModelRecords", [](const std::string& j_str, smtk::model::ResourcePtr rsrc){ nlohmann::json j = nlohmann::json::parse(j_str); return smtk::model::SessionIOJSON::loadModelRecords(j, rsrc); })
    .def_static("saveJSON", [](const smtk::model::ResourcePtr& rsrc){ nlohmann::json j = smtk::model::SessionIOJSON::saveJSON(rsrc); return j.dump(4); })
    ;
  return instance;
}

#endif
