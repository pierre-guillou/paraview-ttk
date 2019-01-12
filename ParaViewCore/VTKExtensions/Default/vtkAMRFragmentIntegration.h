/*=========================================================================

  Program:   ParaView
  Module:    vtkAMRFragmentIntegration.h

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notice for more information.

  Copyright 2013 Sandia Corporation.
  Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
  the U.S. Government retains certain rights in this software.

=========================================================================*/
/**
 * @class   vtkAMRFragmentIntegration
 * @brief   Generates fragment analysis from an
 * amr volume and a previously run contour on that volume
 *
 *
 *   Input 0: The AMR Volume
 *
 *   Output 0: A multiblock containing tables of fragments, one block
 *             for each requested material
*/

#ifndef vtkAMRFragmentIntegration_h
#define vtkAMRFragmentIntegration_h

#include "vtkMultiBlockDataSetAlgorithm.h"
#include "vtkPVVTKExtensionsDefaultModule.h" //needed for exports
#include <string>                            // STL required.
#include <vector>                            // STL required.

class vtkTable;
class vtkNonOverlappingAMR;
class vtkDataSet;

class VTKPVVTKEXTENSIONSDEFAULT_EXPORT vtkAMRFragmentIntegration
  : public vtkMultiBlockDataSetAlgorithm
{
public:
  static vtkAMRFragmentIntegration* New();
  vtkTypeMacro(vtkAMRFragmentIntegration, vtkMultiBlockDataSetAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

protected:
  vtkAMRFragmentIntegration();
  ~vtkAMRFragmentIntegration() override;

  int FillInputPortInformation(int port, vtkInformation* info) VTK_OVERRIDE;
  int FillOutputPortInformation(int port, vtkInformation* info) VTK_OVERRIDE;

  int RequestData(vtkInformation*, vtkInformationVector**, vtkInformationVector*) VTK_OVERRIDE;

  /**
   * Pipeline helper.  Run on each material independently.
   */
  vtkTable* DoRequestData(vtkNonOverlappingAMR* volume, const char* volumeArray,
    const char* massArray, std::vector<std::string> volumeWeightedNames,
    std::vector<std::string> massWeightedNames);

private:
  vtkAMRFragmentIntegration(const vtkAMRFragmentIntegration&) = delete;
  void operator=(const vtkAMRFragmentIntegration&) = delete;
};

#endif /* vtkAMRFragmentIntegration_h */
//
// VTK-HeaderTest-Exclude: vtkAMRFragmentIntegration.h
