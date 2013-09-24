/*=========================================================================

Copyright (c) 1998-2005 Kitware Inc. 28 Corporate Drive, Suite 204,
Clifton Park, NY, 12065, USA.

All rights reserved. No part of this software may be reproduced,
distributed,
or modified, in any form or by any means, without permission in writing from
Kitware Inc.

IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY FOR
DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY DERIVATIVES THEREOF,
EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE IS PROVIDED ON AN
"AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE NO OBLIGATION TO
PROVIDE
MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

=========================================================================*/
// .NAME vtkModelGeneratedGridRepresentation - Abstract class for a generated
//  CMBModel representation of an analysis grid.
// .SECTION Description
// An abstract class used to provide all of the information that a CMBModel needs
// to keep track of mapping grid objects from the geometry grid to the
// analysis grid. In this case the analysis grid isn't read in from a file
// but instead is generated by some code.

#ifndef __vtkModelGeneratedGridRepresentation_h
#define __vtkModelGeneratedGridRepresentation_h

#include "vtkDiscreteModelModule.h" // For export macro
#include "vtkModelGridRepresentation.h"
#include <vector>

class VTKDISCRETEMODEL_EXPORT vtkModelGeneratedGridRepresentation : public vtkModelGridRepresentation
{
public:
  vtkTypeMacro(vtkModelGeneratedGridRepresentation,vtkModelGridRepresentation);
  void PrintSelf(ostream& os, vtkIndent indent);

  virtual void WriteMeshToFile()=0;

  // Description:
  // Get cellIds or area given a group id of model entities.
  // Meant for 2D models with triangle meshes.
  virtual bool GetGroupFacetIds(vtkDiscreteModel* model,int groupId,
    std::vector<int>& cellIds){return false;}
  virtual bool GetGroupFacetsArea(vtkDiscreteModel* model,int groupId, double& area)
  {return false;}

  // Description:
  // Get the locations of points and its containing cells given the group id
  // of model enities. These points are randomly placed inside of the randomly
  // located cell withint the model group.
  // Meant for 2D models with triangle meshes.
  virtual bool GetRandomPointsInGroupDomain(
    vtkDiscreteModel* model, int groupId, int numberOfAgents,
    std::vector<std::pair<int, std::pair<double, double> > >& locations)
  {return false;}

protected:
  vtkModelGeneratedGridRepresentation();
  virtual ~vtkModelGeneratedGridRepresentation();

private:
  vtkModelGeneratedGridRepresentation(const vtkModelGeneratedGridRepresentation&);  // Not implemented.
  void operator=(const vtkModelGeneratedGridRepresentation&);  // Not implemented.
};
#endif
