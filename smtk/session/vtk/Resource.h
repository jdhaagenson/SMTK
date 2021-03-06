//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================
#ifndef __smtk_session_vtk_Resource_h
#define __smtk_session_vtk_Resource_h

#include "smtk/session/vtk/Exports.h"
#include "smtk/session/vtk/Session.h"

#include "smtk/model/Resource.h"

#include "smtk/resource/DerivedFrom.h"
#include "smtk/resource/Manager.h"

namespace smtk
{
namespace session
{
namespace vtk
{

class SMTKVTKSESSION_EXPORT Resource
  : public smtk::resource::DerivedFrom<Resource, smtk::model::Resource>
{
public:
  smtkTypeMacro(smtk::session::vtk::Resource);
  smtkSharedPtrCreateMacro(smtk::resource::Resource);

  virtual ~Resource() {}

  const Session::Ptr& session() const { return m_session; }
  void setSession(const Session::Ptr&);

protected:
  Resource(const smtk::common::UUID&, smtk::resource::Manager::Ptr manager = nullptr);
  Resource(smtk::resource::Manager::Ptr manager = nullptr);

  Session::Ptr m_session;
};

} // namespace vtk
} // namespace session
} // namespace smtk

#endif // __smtk_session_vtk_Resource_h
