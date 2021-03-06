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
#include "smtk/session/rgg/Session.h"

namespace smtk
{
namespace session
{
namespace rgg
{
typedef smtk::model::SessionInfoBits SessionInfoBits;

Session::Session()
{
}

SessionInfoBits Session::transcribeInternal(
  const model::EntityRef& entity, SessionInfoBits requestedInfo, int depth)
{
  (void)entity;
  (void)depth;
  (void)requestedInfo;
  return smtk::model::SESSION_EVERYTHING;
}
}
}
}
