/*=========================================================================

  Program:   ParaView
  Module:    vtkPVSelectionInformation.h

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkPVSelectionInformation
 * @brief   Used to gather selection information
 *
 * Used to get information about selection from server to client.
 * The results are stored in a vtkSelection.
 * @sa
 * vtkSelection
*/

#ifndef vtkPVSelectionInformation_h
#define vtkPVSelectionInformation_h

#include "vtkPVClientServerCoreRenderingModule.h" //needed for exports
#include "vtkPVInformation.h"

class vtkClientServerStream;
class vtkPVXMLElement;
class vtkSelection;

class VTKPVCLIENTSERVERCORERENDERING_EXPORT vtkPVSelectionInformation : public vtkPVInformation
{
public:
  static vtkPVSelectionInformation* New();
  vtkTypeMacro(vtkPVSelectionInformation, vtkPVInformation);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  /**
   * Copy information from a selection to internal datastructure.
   */
  void CopyFromObject(vtkObject*) VTK_OVERRIDE;

  /**
   * Merge another information object.
   */
  void AddInformation(vtkPVInformation*) VTK_OVERRIDE;

  //@{
  /**
   * Manage a serialized version of the information.
   */
  void CopyToStream(vtkClientServerStream*) VTK_OVERRIDE;
  void CopyFromStream(const vtkClientServerStream*) VTK_OVERRIDE;
  //@}

  //@{
  /**
   * Returns the selection. Selection is created and populated
   * at the end of GatherInformation.
   */
  vtkGetObjectMacro(Selection, vtkSelection);
  //@}

protected:
  vtkPVSelectionInformation();
  ~vtkPVSelectionInformation() override;

  void Initialize();
  vtkSelection* Selection;

private:
  vtkPVSelectionInformation(const vtkPVSelectionInformation&) = delete;
  void operator=(const vtkPVSelectionInformation&) = delete;
};

#endif
