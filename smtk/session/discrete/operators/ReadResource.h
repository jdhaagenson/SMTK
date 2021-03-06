//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================

#ifndef __smtk_session_discrete_ReadResource_h
#define __smtk_session_discrete_ReadResource_h

#include "smtk/session/discrete/Exports.h"

#include "smtk/operation/XMLOperation.h"

namespace smtk
{
namespace session
{
namespace discrete
{

/**\brief Read an smtk discrete model file.
  */
class SMTKDISCRETESESSION_EXPORT ReadResource : public smtk::operation::XMLOperation
{
public:
  smtkTypeMacro(smtk::session::discrete::ReadResource);
  smtkCreateMacro(ReadResource);
  smtkSharedFromThisMacro(smtk::operation::Operation);

protected:
  Result operateInternal() override;
  virtual const char* xmlDescription() const override;
  void markModifiedResources(Result&) override;
};

SMTKDISCRETESESSION_EXPORT smtk::resource::ResourcePtr readResource(const std::string&);
}
}
}

#endif
