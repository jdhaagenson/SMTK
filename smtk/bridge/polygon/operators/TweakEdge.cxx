//=============================================================================
// Copyright (c) Kitware, Inc.
// All rights reserved.
// See LICENSE.txt for details.
//
// This software is distributed WITHOUT ANY WARRANTY; without even
// the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE.  See the above copyright notice for more information.
//=============================================================================
#include "smtk/bridge/polygon/operators/TweakEdge.h"

#include "smtk/bridge/polygon/Session.h"
#include "smtk/bridge/polygon/internal/Model.h"
#include "smtk/bridge/polygon/internal/Model.txx"

#include "smtk/io/Logger.h"

#include "smtk/model/Vertex.h"
#include "smtk/model/Face.h"

#include "smtk/attribute/Attribute.h"
#include "smtk/attribute/DoubleItem.h"
#include "smtk/attribute/IntItem.h"
#include "smtk/attribute/ModelEntityItem.h"
#include "smtk/attribute/StringItem.h"

#include "smtk/bridge/polygon/TweakEdge_xml.h"

namespace smtk {
  namespace bridge {
    namespace polygon {

smtk::model::OperatorResult TweakEdge::operateInternal()
{
  smtk::bridge::polygon::Session* sess = this->polygonSession();
  smtk::model::Manager::Ptr mgr;
  if (!sess)
    {
    smtkErrorMacro(this->log(), "Invalid session.");
    return this->createResult(smtk::model::OPERATION_FAILED);
    }

  mgr = sess->manager();

  smtk::attribute::ModelEntityItem::Ptr edgeItem = this->specification()->associations();
  smtk::attribute::DoubleItem::Ptr pointsItem = this->findDouble("points");
  smtk::attribute::IntItem::Ptr coordinatesItem = this->findInt("coordinates");
  smtk::attribute::IntItem::Ptr promoteItem = this->findInt("promote");

  smtk::model::Edge src(edgeItem->value(0));
  if (!src.isValid())
    {
    smtkErrorMacro(this->log(), "Input edge was invalid.");
    return this->createResult(smtk::model::OPERATION_FAILED);
    }

  internal::edge::Ptr storage = this->findStorage<internal::edge>(src.entity());
  internal::pmodel* pmod = storage->parentAs<internal::pmodel>();
  if (!storage || !pmod)
    {
    smtkErrorMacro(this->log(), "Input edge was not part of its parent model (or not a polygon-session edge).");
    return this->createResult(smtk::model::OPERATION_FAILED);
    }

  bool ok = true;
  std::set<int> splits(promoteItem->begin(), promoteItem->end());
  int numCoordsPerPt = coordinatesItem->value(0);
  std::size_t npts = pointsItem->numberOfValues() / numCoordsPerPt; // == #pts / #coordsPerPt
  if (npts < 2)
    {
    smtkErrorMacro(this->log(),
      "Not enough points to form an edge (" << pointsItem->numberOfValues()
      << " coordinates at " << numCoordsPerPt << " per point => "
      << npts << " points)");
    return this->createResult(smtk::model::OPERATION_FAILED);
    }

  if (!splits.empty())
    {
    std::set<int>::iterator sit;
    std::ostringstream derp;
    bool bad = false;
    derp << "Ignoring invalid split-point indices: ";
    for (sit = splits.begin(); sit != splits.end() && *sit < 0; ++sit)
      {
      derp << " " << *sit;
      std::set<int>::iterator tmp = sit;
      ++sit;
      splits.erase(tmp);
      bad = true;
      --sit;
      }
    std::set<int>::reverse_iterator rsit;
    for (rsit = splits.rbegin(); rsit != splits.rend() && *rsit >= npts; ++rsit)
      {
      derp << " " << *rsit;
      std::set<int>::iterator tmp = --rsit.base();
      ++rsit;
      splits.erase(tmp);
      bad = true;
      --rsit;
      }
    if (bad)
      {
      smtkWarningMacro(this->log(), derp.str());
      }
    }

  // Determine whether the original edge was periodic
  internal::PointSeq& epts(storage->points());
  bool isPeriodic = (*epts.begin()) == (*(++epts.rbegin()).base());

  smtk::model::EntityRefArray modified; // includes edge and perhaps eventually faces.
  smtk::model::Edges created;

  // Done checking input. Perform operation.
  // See which model vertex (if any) matches the existing begin
  smtk::model::Vertices verts = src.vertices();
  bool isFirstVertStart;
  if (!verts.empty())
    {
    internal::vertex::Ptr firstVert = this->findStorage<internal::vertex>(verts.begin()->entity());
    isFirstVertStart = (firstVert->point() == *epts.begin());
    }
  // Now erase the existing edge points and rewrite them:
  epts.clear();
  std::vector<double>::const_iterator coordit = pointsItem->begin();
  for (std::size_t p = 0; p < npts; ++p)
    {
    internal::Point proj = pmod->projectPoint(coordit, coordit + numCoordsPerPt);
    epts.insert(epts.end(), proj);
    coordit += numCoordsPerPt;
    }
  if (isPeriodic && (*epts.begin()) != (*(++epts.rbegin()).base()))
    { // It was periodic but isn't any more. Close the loop naively.
    epts.insert(epts.end(), *epts.begin());
    }
  // Lift the integer points into world coordinates:
  pmod->addEdgeTessellation(src, storage);
  modified.push_back(src);

  smtk::model::EntityRefs modEdgesAndFaces;
  for (smtk::model::Vertices::iterator vit = verts.begin(); vit != verts.end(); ++vit)
    {
    internal::Point locn = ((vit == verts.begin()) != !isFirstVertStart) ? *epts.begin() :  *(++epts.rbegin()).base();
    //internal::Point locn = (vit == verts.begin() ? *epts.begin() :  *(++epts.rbegin()).base());
    if (pmod->tweakVertex(*vit, locn, modEdgesAndFaces))
      {
      modified.push_back(*vit);
      }
    }
  modified.insert(modified.end(), modEdgesAndFaces.begin(), modEdgesAndFaces.end());
  // If the edge had no model vertices, then we must see if any faces are attached to it
  // and update their tessellations. (If it did have verts, tweakVertex will have updated them.)
  if (verts.empty())
    {
    smtk::model::Faces facesOnEdge = src.faces();
    for (smtk::model::Faces::iterator fit = facesOnEdge.begin(); fit != facesOnEdge.end(); ++fit)
      {
      // If we have a face attached, re-tessellate it and add to modEdgesAndFaces
      if (modEdgesAndFaces.find(*fit) == modEdgesAndFaces.end())
        {
        pmod->addFaceTessellation(*fit);
        modEdgesAndFaces.insert(*fit);
        }
      }
    }

  // Split the edge as requested by the user:
  std::vector<internal::vertex::Ptr> dummy;
  std::vector<internal::PointSeq::const_iterator> splitLocs;
  internal::PointSeq::const_iterator ptit = epts.begin();
  int last = 0;
  for (std::set<int>::iterator promit = splits.begin(); promit != splits.end(); ++promit)
    {
    std::advance(ptit, *promit - last);
    last = *promit;
    dummy.push_back(this->findStorage<internal::vertex>(pmod->findOrAddModelVertex(mgr, *ptit).entity()));
    splitLocs.push_back(ptit);
    }
  smtk::model::EntityRefs eset;
  smtk::model::EntityRefs emod;
  if (!pmod->splitModelEdgeAtModelVertices(mgr, storage, dummy, splitLocs, eset, emod))
    {
    smtkErrorMacro(this->log(), "Could not split edge.");
    ok = false;
    }
  smtk::model::EntityRefArray expunged;
  if (!eset.empty())
    {
    expunged.push_back(src);
    }
  created.insert(created.end(), eset.begin(), eset.end());
  modified.insert(modified.end(), emod.begin(), emod.end());

  smtk::model::OperatorResult opResult;
  if (ok)
    {
    opResult = this->createResult(smtk::model::OPERATION_SUCCEEDED);
    this->addEntitiesToResult(opResult, expunged, EXPUNGED);
    this->addEntitiesToResult(opResult, created, CREATED);
    this->addEntitiesToResult(opResult, modified, MODIFIED);

    // Modified items will have new tessellations, which we must indicate
    // separately for the time being.
    attribute::ModelEntityItemPtr tessItem = opResult->findModelEntity("tess_changed");
    tessItem->appendValues(modified.begin(), modified.end());
    }
  else
    {
    opResult = this->createResult(smtk::model::OPERATION_FAILED);
    }

  return opResult;
}

    } // namespace polygon
  } //namespace bridge
} // namespace smtk

smtkImplementsModelOperator(
  SMTKPOLYGONSESSION_EXPORT,
  smtk::bridge::polygon::TweakEdge,
  polygon_tweak_edge,
  "tweak edge",
  TweakEdge_xml,
  smtk::bridge::polygon::Session);
