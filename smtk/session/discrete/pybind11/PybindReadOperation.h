//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================

#ifndef pybind_smtk_session_discrete_operators_ReadOperation_h
#define pybind_smtk_session_discrete_operators_ReadOperation_h

#include <pybind11/pybind11.h>

#include "smtk/session/discrete/operators/ReadOperation.h"


namespace py = pybind11;

PySharedPtrClass< smtk::session::discrete::ReadOperation, smtk::operation::Operation > pybind11_init_smtk_session_discrete_ReadOperation(py::module &m)
{
  PySharedPtrClass< smtk::session::discrete::ReadOperation, smtk::operation::Operation > instance(m, "ReadOperation");
  instance
    .def_static("create", (std::shared_ptr<smtk::session::discrete::ReadOperation> (*)()) &smtk::session::discrete::ReadOperation::create)
    .def("ableToOperate", &smtk::session::discrete::ReadOperation::ableToOperate)
    ;
  return instance;
}

#endif
