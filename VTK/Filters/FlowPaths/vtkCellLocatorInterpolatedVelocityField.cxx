/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkCellLocatorInterpolatedVelocityField.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// VTK_DEPRECATED_IN_9_2_0() warnings for this class.
#define VTK_DEPRECATION_LEVEL 0

#include "vtkCellLocatorInterpolatedVelocityField.h"

#include "vtkCellLocatorStrategy.h"
#include "vtkObjectFactory.h"

vtkStandardNewMacro(vtkCellLocatorInterpolatedVelocityField);

//------------------------------------------------------------------------------
vtkCellLocatorInterpolatedVelocityField::vtkCellLocatorInterpolatedVelocityField()
{
  // Create the default FindCellStrategy. Note that it is deleted by the
  // superclass.
  this->FindCellStrategy = vtkCellLocatorStrategy::New();
}

//------------------------------------------------------------------------------
vtkCellLocatorInterpolatedVelocityField::~vtkCellLocatorInterpolatedVelocityField() = default;

//------------------------------------------------------------------------------
void vtkCellLocatorInterpolatedVelocityField::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
