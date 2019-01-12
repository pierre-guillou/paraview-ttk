#include "FEAdaptor.h"
#include "FEDataStructures.h"
#include <iostream>

#include <vtkCPDataDescription.h>
#include <vtkCPInputDataDescription.h>
#include <vtkCPProcessor.h>
#include <vtkCPPythonScriptPipeline.h>
#include <vtkCellData.h>
#include <vtkCellType.h>
#include <vtkDoubleArray.h>
#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>

namespace
{
vtkCPProcessor* Processor = NULL;
vtkImageData* VTKGrid = NULL;

void BuildVTKGrid(Grid& grid)
{
  // The grid structure isn't changing so we only build it
  // the first time it's needed. If we needed the memory
  // we could delete it and rebuild as necessary.
  if (VTKGrid == NULL)
  {
    VTKGrid = vtkImageData::New();
    int extent[6];
    for (int i = 0; i < 6; i++)
    {
      extent[i] = grid.GetExtent()[i];
    }
    VTKGrid->SetExtent(extent);
    VTKGrid->SetSpacing(grid.GetSpacing());
  }
}

void UpdateVTKAttributes(Grid& grid, Attributes& attributes, vtkCPInputDataDescription* idd)
{
  if (idd->IsFieldNeeded("velocity", vtkDataObject::POINT) == true)
  {
    if (VTKGrid->GetPointData()->GetNumberOfArrays() == 0)
    {
      // velocity array
      vtkNew<vtkDoubleArray> velocity;
      velocity->SetName("velocity");
      velocity->SetNumberOfComponents(3);
      velocity->SetNumberOfTuples(static_cast<vtkIdType>(grid.GetNumberOfLocalPoints()));
      VTKGrid->GetPointData()->AddArray(velocity.GetPointer());
    }
    vtkDoubleArray* velocity =
      vtkDoubleArray::SafeDownCast(VTKGrid->GetPointData()->GetArray("velocity"));
    // The velocity array is ordered as vx0,vx1,vx2,..,vy0,vy1,vy2,..,vz0,vz1,vz2,..
    // so we need to create a full copy of it with VTK's ordering of
    // vx0,vy0,vz0,vx1,vy1,vz1,..
    double* velocityData = attributes.GetVelocityArray();
    vtkIdType numTuples = velocity->GetNumberOfTuples();
    for (vtkIdType i = 0; i < numTuples; i++)
    {
      double values[3] = { velocityData[i], velocityData[i + numTuples],
        velocityData[i + 2 * numTuples] };
      velocity->SetTypedTuple(i, values);
    }
  }
  if (idd->IsFieldNeeded("pressure", vtkDataObject::CELL) == true)
  {
    if (VTKGrid->GetCellData()->GetNumberOfArrays() == 0)
    {
      // pressure array
      vtkNew<vtkFloatArray> pressure;
      pressure->SetName("pressure");
      pressure->SetNumberOfComponents(1);
      VTKGrid->GetCellData()->AddArray(pressure.GetPointer());
    }
    vtkFloatArray* pressure =
      vtkFloatArray::SafeDownCast(VTKGrid->GetCellData()->GetArray("pressure"));
    // The pressure array is a scalar array so we can reuse
    // memory as long as we ordered the points properly.
    float* pressureData = attributes.GetPressureArray();
    pressure->SetArray(pressureData, static_cast<vtkIdType>(grid.GetNumberOfLocalCells()), 1);
  }
}

void BuildVTKDataStructures(Grid& grid, Attributes& attributes, vtkCPInputDataDescription* idd)
{
  BuildVTKGrid(grid);
  UpdateVTKAttributes(grid, attributes, idd);
}
}

namespace FEAdaptor
{

void Initialize(int numScripts, char* scripts[])
{
  if (Processor == NULL)
  {
    Processor = vtkCPProcessor::New();
    Processor->Initialize();
  }
  else
  {
    Processor->RemoveAllPipelines();
  }
  for (int i = 0; i < numScripts; i++)
  {
    vtkNew<vtkCPPythonScriptPipeline> pipeline;
    pipeline->Initialize(scripts[i]);
    Processor->AddPipeline(pipeline.GetPointer());
  }
}

void Finalize()
{
  if (Processor)
  {
    Processor->Delete();
    Processor = NULL;
  }
  if (VTKGrid)
  {
    VTKGrid->Delete();
    VTKGrid = NULL;
  }
}

void CoProcess(
  Grid& grid, Attributes& attributes, double time, unsigned int timeStep, bool lastTimeStep)
{
  vtkNew<vtkCPDataDescription> dataDescription;
  dataDescription->AddInput("input");
  dataDescription->SetTimeData(time, timeStep);
  if (lastTimeStep == true)
  {
    // assume that we want to all the pipelines to execute if it
    // is the last time step.
    dataDescription->ForceOutputOn();
  }
  if (Processor->RequestDataDescription(dataDescription.GetPointer()) != 0)
  {
    vtkCPInputDataDescription* idd = dataDescription->GetInputDescriptionByName("input");
    BuildVTKDataStructures(grid, attributes, idd);
    idd->SetGrid(VTKGrid);
    int wholeExtent[6];
    for (int i = 0; i < 3; i++)
    {
      wholeExtent[2 * i] = 0;
      wholeExtent[2 * i + 1] = grid.GetNumPoints()[i];
    }

    dataDescription->GetInputDescriptionByName("input")->SetWholeExtent(wholeExtent);
    Processor->CoProcess(dataDescription.GetPointer());
  }
}
} // end of Catalyst namespace
