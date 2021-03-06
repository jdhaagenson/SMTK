//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================

// .NAME vtkCMBSTLReader - Read in a stl file into CMB.
// .SECTION Description
// Read in a stl file into CMB.

#ifndef __smtk_vtk_vtkCMBSTLReader_h
#define __smtk_vtk_vtkCMBSTLReader_h

#include "smtk/extension/vtk/reader/Exports.h" // For export macro
#include "vtkPolyDataAlgorithm.h"

class VTKSMTKREADEREXT_EXPORT vtkCMBSTLReader : public vtkPolyDataAlgorithm
{
public:
  static vtkCMBSTLReader* New();
  vtkTypeMacro(vtkCMBSTLReader, vtkPolyDataAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  // Description:
  // Get/Set the name of the input file.
  vtkSetStringMacro(FileName);
  vtkGetStringMacro(FileName);

protected:
  vtkCMBSTLReader();
  ~vtkCMBSTLReader() override;

  int RequestInformation(vtkInformation*, vtkInformationVector**, vtkInformationVector*) override;
  int RequestData(vtkInformation*, vtkInformationVector**, vtkInformationVector*) override;

private:
  vtkCMBSTLReader(const vtkCMBSTLReader&); // Not implemented.
  void operator=(const vtkCMBSTLReader&);  // Not implemented.

  // Description:
  // The name of the file to be read in.
  char* FileName;
};

#endif
