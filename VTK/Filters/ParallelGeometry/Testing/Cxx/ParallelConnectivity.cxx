/*=========================================================================

  Program:   Visualization Toolkit
  Module:    ParallelConnectivity.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkConnectivityFilter.h"

#include "vtkContourFilter.h"
#include "vtkDataSetTriangleFilter.h"
#include "vtkDistributedDataFilter.h"
#include "vtkMPIController.h"
#include "vtkPUnstructuredGridGhostCellsGenerator.h"
#include "vtkPConnectivityFilter.h"
#include "vtkRemoveGhosts.h"
#include "vtkStructuredPoints.h"
#include "vtkStructuredPointsReader.h"
#include "vtkTestUtilities.h"
#include "vtkUnstructuredGrid.h"

#include <mpi.h>

int RunParallelConnectivity(const char* fname, vtkAlgorithm::DesiredOutputPrecision precision, vtkMPIController* contr)
{
  int returnValue = EXIT_SUCCESS;
  int me = contr->GetLocalProcessId();

  vtkNew<vtkStructuredPointsReader> reader;
  vtkDataSet* ds;
  vtkSmartPointer<vtkUnstructuredGrid> ug = vtkSmartPointer<vtkUnstructuredGrid>::New();
  if (me == 0)
  {
    std::cout << fname << std::endl;
    reader->SetFileName(fname);
    reader->Update();

    ds = reader->GetOutput();
  }
  else
  {
    ds = ug;
  }

  vtkNew<vtkDistributedDataFilter> dd;
  dd->SetInputData(ds);
  dd->SetController(contr);
  dd->UseMinimalMemoryOff();
  dd->SetBoundaryModeToAssignToOneRegion();

  vtkNew<vtkContourFilter> contour;
  contour->SetInputConnection(dd->GetOutputPort());
  contour->SetNumberOfContours(1);
  contour->SetOutputPointsPrecision(precision);
  contour->SetValue(0, 240.0);

  vtkNew<vtkDataSetTriangleFilter> tetrahedralize;
  tetrahedralize->SetInputConnection(contour->GetOutputPort());

  vtkNew<vtkPUnstructuredGridGhostCellsGenerator> ghostCells;
  ghostCells->SetController(contr);
  ghostCells->SetBuildIfRequired(false);
  ghostCells->SetMinimumNumberOfGhostLevels(1);
  ghostCells->SetInputConnection(tetrahedralize->GetOutputPort());

  // Test factory override mechanism instantiated as a vtkPConnectivityFilter.
  vtkNew<vtkConnectivityFilter> connectivity;
  if (connectivity->IsA("vtkConnectivityFiltetr"))
  {
    std::cerr << "Expected vtkConnectivityFilter filter to be instantiated "
              << "as a vtkPConnectivityFilter with MPI support enabled, but "
              << "it is a " << connectivity->GetClassName() << " instead." << std::endl;
  }

  connectivity->SetInputConnection(ghostCells->GetOutputPort());
  connectivity->Update();

  // Remove ghost points/cells so that the cell count is the same regardless
  // of the number of processes.
  vtkNew<vtkRemoveGhosts> removeGhosts;
  removeGhosts->SetInputConnection(connectivity->GetOutputPort());

  // Check the number of regions
  int numberOfRegions = connectivity->GetNumberOfExtractedRegions();
  int expectedNumberOfRegions = 19;
  if (numberOfRegions != expectedNumberOfRegions)
  {
    std::cerr << "Expected " << expectedNumberOfRegions << " regions but got "
      << numberOfRegions << std::endl;
    returnValue = EXIT_FAILURE;
  }

  // Check the number of cells in the largest region when the extraction mode
  // is set to largest region.
  connectivity->SetExtractionModeToLargestRegion();
  removeGhosts->Update();
  int numberOfCells =
    vtkPointSet::SafeDownCast(removeGhosts->GetOutput())->GetNumberOfCells();
  int globalNumberOfCells = 0;
  contr->AllReduce(&numberOfCells, &globalNumberOfCells, 1, vtkCommunicator::SUM_OP);

  int expectedNumberOfCells = 2124;
  if (globalNumberOfCells != expectedNumberOfCells)
  {
    std::cerr << "Expected " << expectedNumberOfCells << " cells in largest "
      << "region bug got " << globalNumberOfCells << std::endl;
    returnValue = EXIT_FAILURE;
  }

  // Closest point region test
  connectivity->SetExtractionModeToClosestPointRegion();
  removeGhosts->Update();
  numberOfCells =
    vtkPointSet::SafeDownCast(removeGhosts->GetOutput())->GetNumberOfCells();
  contr->AllReduce(&numberOfCells, &globalNumberOfCells, 1, vtkCommunicator::SUM_OP);
  expectedNumberOfCells = 862; // point (0, 0, 0)
  if (globalNumberOfCells != expectedNumberOfCells)
  {
    std::cerr << "Expected " << expectedNumberOfCells << " cells in closest "
      << "point extraction mode but got " << globalNumberOfCells << std::endl;
    returnValue = EXIT_FAILURE;
  }

  return returnValue;
}

int ParallelConnectivity(int argc, char* argv[])
{
  int returnValue = EXIT_SUCCESS;

  MPI_Init(&argc, &argv);

  // Note that this will create a vtkMPIController if MPI
  // is configured, vtkThreadedController otherwise.
  vtkMPIController *contr = vtkMPIController::New();
  contr->Initialize(&argc, &argv, 1);

  vtkMultiProcessController::SetGlobalController(contr);

  char* fname =
    vtkTestUtilities::ExpandDataFileName(argc, argv, "Data/ironProt.vtk");

  if (RunParallelConnectivity(fname, vtkAlgorithm::SINGLE_PRECISION, contr) != EXIT_SUCCESS)
  {
    std::cerr << "Error running with vtkAlgorithm::SINGLE_PRECISION" << std::endl;
    returnValue = EXIT_FAILURE;
  }
  if (RunParallelConnectivity(fname, vtkAlgorithm::DOUBLE_PRECISION, contr) != EXIT_SUCCESS)
  {
    std::cerr << "Error running with vtkAlgorithm::DOUBLE_PRECISION" << std::endl;
    returnValue = EXIT_FAILURE;
  }

  delete[] fname;

  contr->Finalize();
  contr->Delete();

  return returnValue;
}
