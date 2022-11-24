/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkScalarBarRepresentation.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

/*
 * Copyright 2008 Sandia Corporation.
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the
 * U.S. Government. Redistribution and use in source and binary forms, with
 * or without modification, are permitted provided that this Notice and any
 * statement of authorship are reproduced on all copies.
 */

#include "vtkPVScalarBarRepresentation.h"

#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkViewport.h"

#include "vtkContext2DScalarBarActor.h"

//-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkPVScalarBarRepresentation);

//-----------------------------------------------------------------------------
int vtkPVScalarBarRepresentation::RenderOverlay(vtkViewport* viewport)
{
  // Query scalar bar size given the viewport
  vtkContext2DScalarBarActor* actor =
    vtkContext2DScalarBarActor::SafeDownCast(this->GetScalarBarActor());
  if (!actor)
  {
    vtkErrorMacro(<< "Actor expected to be of type vtkContext2DScalarBarActor");
    return 0;
  }

  vtkRectf boundingRect = actor->GetBoundingRect();

  // Start with Lower Right corner.
  int* displaySize = viewport->GetSize();

  if (this->WindowLocation != vtkBorderRepresentation::AnyLocation)
  {
    double pad = 4.0;
    double x = 0.0;
    double y = 0.0;
    switch (this->WindowLocation)
    {
      case vtkBorderRepresentation::LowerLeftCorner:
        x = 0.0 + pad;
        y = 0.0 + pad;
        break;

      case vtkBorderRepresentation::LowerRightCorner:
        x = displaySize[0] - 1.0 - boundingRect.GetWidth() - pad;
        y = 0.0 + pad;
        break;

      case vtkBorderRepresentation::LowerCenter:
        x = 0.5 * (displaySize[0] - boundingRect.GetWidth());
        y = 0.0 + pad;
        break;

      case vtkBorderRepresentation::UpperLeftCorner:
        x = 0.0 + pad;
        y = displaySize[1] - 1.0 - boundingRect.GetHeight() - pad;
        break;

      case vtkBorderRepresentation::UpperRightCorner:
        x = displaySize[0] - 1.0 - boundingRect.GetWidth() - pad;
        y = displaySize[1] - 1.0 - boundingRect.GetHeight() - pad;
        break;

      case vtkBorderRepresentation::UpperCenter:
        x = 0.5 * (displaySize[0] - boundingRect.GetWidth());
        y = displaySize[1] - 1.0 - boundingRect.GetHeight() - pad;

      default:
        break;
    }

    x -= boundingRect.GetX();
    y -= boundingRect.GetY();

    viewport->DisplayToNormalizedDisplay(x, y);

    this->PositionCoordinate->SetValue(x, y);
  }

  return this->Superclass::RenderOverlay(viewport);
}

//------------------------------------------------------------------------------
void vtkPVScalarBarRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
