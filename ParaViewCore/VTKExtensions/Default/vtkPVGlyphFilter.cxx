/*=========================================================================

  Program:   ParaView
  Module:    vtkPVGlyphFilter.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkPVGlyphFilter.h"

// VTK includes
#include "vtkBoundingBox.h"
#include "vtkCellCenters.h"
#include "vtkCellData.h"
#include "vtkCompositeDataIterator.h"
#include "vtkDataSet.h"
#include "vtkFloatArray.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMinimalStandardRandomSequence.h"
#include "vtkMultiBlockDataSet.h"
#include "vtkMultiProcessController.h"
#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkOctreePointLocator.h"
#include "vtkPointData.h"
#include "vtkPolyData.h"
#include "vtkSmartPointer.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkTransform.h"
#include "vtkTuple.h"
#include "vtkUniformGrid.h"

// C/C++ includes
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <vector>

class vtkPVGlyphFilter::vtkInternals
{
  vtkBoundingBox Bounds;
  double NearestPointRadius;
  std::vector<vtkTuple<double, 3> > Points;
  std::vector<vtkIdType> PointIds;
  size_t NextPointId;

  vtkNew<vtkOctreePointLocator> Locator;

  void SetupLocator(vtkDataSet* ds)
  {
    if (this->Locator->GetDataSet() == ds)
    {
      return;
    }

    this->Locator->Initialize();
    this->Locator->SetDataSet(ds);
    this->Locator->BuildLocator();

    this->PointIds.clear();
    std::set<vtkIdType> pointset;
    for (std::vector<vtkTuple<double, 3> >::iterator iter = this->Points.begin(),
                                                     end = this->Points.end();
         iter != end; ++iter)
    {
      double dist2;
      vtkIdType ptId = this->Locator->FindClosestPointWithinRadius(
        this->NearestPointRadius, iter->GetData(), dist2);
      if (ptId >= 0)
      {
        pointset.insert(ptId);
      }
    }
    this->PointIds.insert(this->PointIds.begin(), pointset.begin(), pointset.end());
    this->NextPointId = 0;
  }

public:
  void Reset()
  {
    this->Bounds.Reset();
    this->Points.clear();
    this->Locator->Initialize();
    this->Locator->SetDataSet(NULL);
  }

  //---------------------------------------------------------------------------
  // Update internal datastructures for the given dataset. This will collect
  // bounds information for all datasets for SPATIALLY_UNIFORM_DISTRIBUTION.
  void UpdateWithDataset(vtkDataSet* ds, vtkPVGlyphFilter* self)
  {
    assert(ds != NULL && self != NULL);
    if (self->GetGlyphMode() != vtkPVGlyphFilter::SPATIALLY_UNIFORM_DISTRIBUTION)
    {
      // nothing to do.
      return;
    }

    // collect local bounds information.
    double bds[6];
    ds->GetBounds(bds);
    if (vtkBoundingBox::IsValid(bds))
    {
      this->Bounds.AddBounds(bds);
    }
  }

  //---------------------------------------------------------------------------
  // Again, primarily for SPATIALLY_UNIFORM_DISTRIBUTION. We sync the bounds
  // information among all ranks.
  // Subsequently, we also build the list of random sample points using the
  // synchronized bounds.
  void SynchronizeGlobalInformation(vtkPVGlyphFilter* self)
  {
    if (self->GetGlyphMode() != vtkPVGlyphFilter::SPATIALLY_UNIFORM_DISTRIBUTION)
    {
      return; // nothing to do.
    }

    vtkMultiProcessController* controller = self->GetController();
    if (controller && controller->GetNumberOfProcesses() > 1)
    {
      double local_min[3] = { VTK_DOUBLE_MAX, VTK_DOUBLE_MAX, VTK_DOUBLE_MAX };
      double local_max[3] = { VTK_DOUBLE_MIN, VTK_DOUBLE_MIN, VTK_DOUBLE_MIN };
      if (this->Bounds.IsValid())
      {
        this->Bounds.GetMinPoint(local_min[0], local_min[1], local_min[2]);
        this->Bounds.GetMaxPoint(local_max[0], local_max[1], local_max[2]);
      }
      double global_min[3], global_max[3];

      controller->AllReduce(local_min, global_min, 3, vtkCommunicator::MIN_OP);
      controller->AllReduce(local_max, global_max, 3, vtkCommunicator::MAX_OP);

      this->Bounds.SetBounds(
        global_min[0], global_max[0], global_min[1], global_max[1], global_min[2], global_max[2]);
    }

    if (!this->Bounds.IsValid())
    {
      return;
    }

    // build up list of points to glyph.
    vtkNew<vtkMinimalStandardRandomSequence> randomGenerator;
    randomGenerator->SetSeed(self->GetSeed());
    this->Points.resize(self->GetMaximumNumberOfSamplePoints());
    for (vtkIdType cc = 0; cc < self->GetMaximumNumberOfSamplePoints(); cc++)
    {
      vtkTuple<double, 3>& tuple = this->Points[cc];
      randomGenerator->Next();
      tuple[0] = randomGenerator->GetRangeValue(
        this->Bounds.GetMinPoint()[0], this->Bounds.GetMaxPoint()[0]);
      randomGenerator->Next();
      tuple[1] = randomGenerator->GetRangeValue(
        this->Bounds.GetMinPoint()[1], this->Bounds.GetMaxPoint()[1]);
      randomGenerator->Next();
      tuple[2] = randomGenerator->GetRangeValue(
        this->Bounds.GetMinPoint()[2], this->Bounds.GetMaxPoint()[2]);
    }

    double l[3];
    this->Bounds.GetLengths(l);

    int dim = (l[0] > 0.0 && l[1] > 0.0 && l[2] > 0.0) ? 3 : 2;

    double volume = std::pow(this->Bounds.GetDiagonalLength(), dim);
    if (volume > 0.0)
    {
      assert(self->GetMaximumNumberOfSamplePoints() > 0);
      double volumePerGlyph = volume / self->GetMaximumNumberOfSamplePoints();
      double delta = std::pow(volumePerGlyph, 1.0 / dim);
      this->NearestPointRadius = delta / 2.0;
    }
    else
    {
      this->NearestPointRadius = 0.0001;
    }
  }

  //---------------------------------------------------------------------------
  inline bool IsPointVisible(vtkDataSet* ds, vtkIdType ptId, vtkPVGlyphFilter* self)
  {
    assert(ds != NULL && self != NULL);
    switch (self->GetGlyphMode())
    {
      case vtkPVGlyphFilter::ALL_POINTS:
        return true;

      case vtkPVGlyphFilter::EVERY_NTH_POINT:
        return self->GetStride() <= 1 || (ptId % self->GetStride()) == 0;

      case vtkPVGlyphFilter::SPATIALLY_UNIFORM_DISTRIBUTION:
        // This will initialize the point locator and build the list of PointIds
        // that should be glyphed.
        this->SetupLocator(ds);

        // since PointIds is a sorted list, and we know that IsPointVisible will
        // be called for in monotonically increasing fashion for a specific ds, we
        // use this->NextPointId to simplify the "contains" check.

        while (
          (this->NextPointId < this->PointIds.size()) && (this->PointIds[this->NextPointId] < ptId))
        {
          // this is need since it is possible (due to ghost cells or other
          // masking employed by vtkGlyph3D) that certain ptIds are never tested
          // since they are rejected earlier on.
          this->NextPointId++;
        }

        if (this->NextPointId < this->PointIds.size() && ptId == this->PointIds[this->NextPointId])
        {
          this->NextPointId++;
          return true;
        }
    }
    return false;
  }
};

vtkStandardNewMacro(vtkPVGlyphFilter);
vtkCxxSetObjectMacro(vtkPVGlyphFilter, Controller, vtkMultiProcessController);
vtkCxxSetObjectMacro(vtkPVGlyphFilter, SourceTransform, vtkTransform);
//-----------------------------------------------------------------------------
vtkPVGlyphFilter::vtkPVGlyphFilter()
  : VectorScaleMode(SCALE_BY_MAGNITUDE)
  , SourceTransform(nullptr)
  , GlyphMode(ALL_POINTS)
  , MaximumNumberOfSamplePoints(5000)
  , Seed(1)
  , Stride(1)
  , Controller(0)
  , Internals(new vtkPVGlyphFilter::vtkInternals())
{
  this->SetController(vtkMultiProcessController::GetGlobalController());
  this->SetNumberOfInputPorts(2);
}

//-----------------------------------------------------------------------------
vtkPVGlyphFilter::~vtkPVGlyphFilter()
{
  this->SetController(nullptr);
  this->SetSourceTransform(nullptr);
  delete this->Internals;
}

//----------------------------------------------------------------------------
// Specify a source object at a specified table location.
void vtkPVGlyphFilter::SetSourceConnection(int id, vtkAlgorithmOutput* algOutput)
{
  if (id < 0)
  {
    vtkErrorMacro("Bad index " << id << " for source.");
    return;
  }

  int numConnections = this->GetNumberOfInputConnections(1);
  if (id < numConnections)
  {
    this->SetNthInputConnection(1, id, algOutput);
  }
  else if (id == numConnections && algOutput)
  {
    this->AddInputConnection(1, algOutput);
  }
  else if (algOutput)
  {
    vtkWarningMacro("The source id provided is larger than the maximum "
                    "source id, using "
      << numConnections << " instead.");
    this->AddInputConnection(1, algOutput);
  }
}

//----------------------------------------------------------------------------
vtkMTimeType vtkPVGlyphFilter::GetMTime()
{
  vtkMTimeType mTime = this->Superclass::GetMTime();
  vtkMTimeType time;
  if (this->SourceTransform != nullptr)
  {
    time = this->SourceTransform->GetMTime();
    mTime = (time > mTime ? time : mTime);
  }
  return mTime;
}

//----------------------------------------------------------------------------
int vtkPVGlyphFilter::FillInputPortInformation(int port, vtkInformation* info)
{
  if (port == 0)
  {
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
    // we can handle composite datasets.
    info->Append(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkCompositeDataSet");
    return 1;
  }
  else if (port == 1)
  {
    info->Set(vtkAlgorithm::INPUT_IS_REPEATABLE(), 1);
    info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkPolyData");
    return 1;
  }

  return 0;
}

//----------------------------------------------------------------------------
int vtkPVGlyphFilter::FillOutputPortInformation(int vtkNotUsed(port), vtkInformation* info)
{
  // now add our info
  info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkDataObject");
  return 1;
}

//----------------------------------------------------------------------------
int vtkPVGlyphFilter::ProcessRequest(
  vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  if (request->Has(vtkDemandDrivenPipeline::REQUEST_DATA_OBJECT()))
  {
    return this->RequestDataObject(request, inputVector, outputVector);
  }

  return this->Superclass::ProcessRequest(request, inputVector, outputVector);
}

//----------------------------------------------------------------------------
int vtkPVGlyphFilter::RequestDataObject(
  vtkInformation*, vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  if (vtkCompositeDataSet::GetData(inputVector[0], 0))
  {
    vtkMultiBlockDataSet* mds = vtkMultiBlockDataSet::GetData(outputVector, 0);
    if (mds == NULL)
    {
      mds = vtkMultiBlockDataSet::New();
      outputVector->GetInformationObject(0)->Set(vtkDataObject::DATA_OBJECT(), mds);
      mds->FastDelete();
    }
  }
  else
  {
    vtkPolyData* pd = vtkPolyData::GetData(outputVector, 0);
    if (pd == NULL)
    {
      pd = vtkPolyData::New();
      outputVector->GetInformationObject(0)->Set(vtkDataObject::DATA_OBJECT(), pd);
      pd->FastDelete();
    }
  }
  return 1;
}

//----------------------------------------------------------------------------
int vtkPVGlyphFilter::RequestData(vtkInformation* vtkNotUsed(request),
  vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  vtkInformationVector* sourceVector = inputVector[1];

  this->Internals->Reset();

  vtkDataSet* ds = vtkDataSet::GetData(inputVector[0], 0);
  vtkCompositeDataSet* cds = vtkCompositeDataSet::GetData(inputVector[0], 0);
  if (ds)
  {
    this->Internals->UpdateWithDataset(ds, this);
    this->Internals->SynchronizeGlobalInformation(this);

    if (!this->IsInputArrayToProcessValid(ds))
    {
      return 1;
    }

    vtkPolyData* outputPD = vtkPolyData::GetData(outputVector);
    assert(outputPD);
    if (this->UseCellCenters(ds))
    {
      return this->ExecuteWithCellCenters(ds, sourceVector, outputPD) ? 1 : 0;
    }
    else
    {
      return this->Execute(ds, sourceVector, outputPD) ? 1 : 0;
    }
  }
  else if (cds)
  {
    vtkMultiBlockDataSet* outputMD = vtkMultiBlockDataSet::GetData(outputVector);
    assert(outputMD);

    outputMD->CopyStructure(cds);

    vtkSmartPointer<vtkCompositeDataIterator> iter;
    iter.TakeReference(cds->NewIterator());
    for (iter->InitTraversal(); !iter->IsDoneWithTraversal(); iter->GoToNextItem())
    {
      vtkDataSet* current = vtkDataSet::SafeDownCast(iter->GetCurrentDataObject());
      if (current)
      {
        this->Internals->UpdateWithDataset(current, this);
      }
    }
    this->Internals->SynchronizeGlobalInformation(this);

    for (iter->InitTraversal(); !iter->IsDoneWithTraversal(); iter->GoToNextItem())
    {
      vtkDataSet* currentDS = vtkDataSet::SafeDownCast(iter->GetCurrentDataObject());
      if (currentDS)
      {
        if (!this->IsInputArrayToProcessValid(currentDS))
        {
          continue;
        }

        bool res;
        vtkNew<vtkPolyData> outputPD;
        if (this->UseCellCenters(currentDS))
        {
          res = this->ExecuteWithCellCenters(currentDS, sourceVector, outputPD.GetPointer());
        }
        else
        {
          res = this->Execute(currentDS, sourceVector, outputPD.GetPointer());
        }
        if (!res)
        {
          vtkErrorMacro("Glyph generation failed for block: " << iter->GetCurrentFlatIndex());
          this->Internals->Reset();
          return 0;
        }
        outputMD->SetDataSet(iter, outputPD.GetPointer());
      }
    }
  }
  this->Internals->Reset();
  return 1;
}

//-----------------------------------------------------------------------------
int vtkPVGlyphFilter::RequestUpdateExtent(vtkInformation* vtkNotUsed(request),
  vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  // get the info objects
  vtkInformation* inInfo = inputVector[0]->GetInformationObject(0);
  vtkInformation* sourceInfo = inputVector[1]->GetInformationObject(0);
  vtkInformation* outInfo = outputVector->GetInformationObject(0);

  if (sourceInfo)
  {
    sourceInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_PIECE_NUMBER(), 0);
    sourceInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_NUMBER_OF_PIECES(), 1);
    sourceInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_NUMBER_OF_GHOST_LEVELS(), 0);
  }
  inInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_PIECE_NUMBER(),
    outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_PIECE_NUMBER()));
  inInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_NUMBER_OF_PIECES(),
    outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_NUMBER_OF_PIECES()));
  inInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_NUMBER_OF_GHOST_LEVELS(),
    outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_NUMBER_OF_GHOST_LEVELS()));
  inInfo->Set(vtkStreamingDemandDrivenPipeline::EXACT_EXTENT(), 1);

  return 1;
}

//-----------------------------------------------------------------------------
int vtkPVGlyphFilter::IsPointVisible(vtkDataSet* ds, vtkIdType ptId)
{
  return this->Internals->IsPointVisible(ds, ptId, this);
}

//-----------------------------------------------------------------------------
bool vtkPVGlyphFilter::IsInputArrayToProcessValid(vtkDataSet* input)
{
  vtkDataArray* scaleArray = this->GetInputArrayToProcess(0, input);
  vtkDataArray* orientationArray = this->GetInputArrayToProcess(1, input);
  int scaleArrayAssociation = this->GetInputArrayAssociation(0, input);
  int orientationArrayAssociation = this->GetInputArrayAssociation(1, input);
  if (scaleArrayAssociation != orientationArrayAssociation && scaleArray && orientationArray)
  {
    std::string scaleArrayType =
      (scaleArrayAssociation == vtkDataObject::FIELD_ASSOCIATION_POINTS ? "point" : "cell");
    std::string orientationArrayType =
      (orientationArrayAssociation == vtkDataObject::FIELD_ASSOCIATION_POINTS ? "point" : "cell");
    vtkWarningMacro(<< "Mismatched attributes:\n"
                    << scaleArray->GetName() << " is a " << scaleArrayType << " attribute whereas "
                    << orientationArray->GetName() << " is a " << orientationArrayType
                    << " attribute.");
    return false;
  }

  return true;
}

//----------------------------------------------------------------------------
vtkPolyData* vtkPVGlyphFilter::GetSource(int idx, vtkInformationVector* sourceInfo)
{
  vtkInformation* info = sourceInfo->GetInformationObject(idx);
  if (!info)
  {
    return nullptr;
  }
  return vtkPolyData::SafeDownCast(info->Get(vtkDataObject::DATA_OBJECT()));
}

//-----------------------------------------------------------------------------
bool vtkPVGlyphFilter::UseCellCenters(vtkDataSet* input)
{
  int inSScalarsAssociation = this->GetInputArrayAssociation(0, input);
  int inVectorsAssociation = this->GetInputArrayAssociation(1, input);
  return inSScalarsAssociation == vtkDataObject::FIELD_ASSOCIATION_CELLS ||
    inVectorsAssociation == vtkDataObject::FIELD_ASSOCIATION_CELLS;
}

//-----------------------------------------------------------------------------
bool vtkPVGlyphFilter::ExecuteWithCellCenters(
  vtkDataSet* input, vtkInformationVector* sourceVector, vtkPolyData* output)
{
  vtkNew<vtkCellCenters> cellCenters;
  cellCenters->SetInputData(input);
  cellCenters->Update();
  input = cellCenters->GetOutput();
  vtkDataArray* inSScalars = input->GetPointData()->GetArray(
    this->GetInputArrayInformation(0)->Get(vtkDataObject::FIELD_NAME()));
  vtkDataArray* inVectors = input->GetPointData()->GetArray(
    this->GetInputArrayInformation(1)->Get(vtkDataObject::FIELD_NAME()));
  return this->Execute(input, sourceVector, output, inSScalars, inVectors);
}

//----------------------------------------------------------------------------
bool vtkPVGlyphFilter::Execute(
  vtkDataSet* input, vtkInformationVector* sourceVector, vtkPolyData* output)
{
  vtkDataArray* scaleArray = this->GetInputArrayToProcess(0, input);
  vtkDataArray* orientArray = this->GetInputArrayToProcess(1, input);
  return this->Execute(input, sourceVector, output, scaleArray, orientArray);
}

//----------------------------------------------------------------------------
bool vtkPVGlyphFilter::Execute(vtkDataSet* input, vtkInformationVector* sourceVector,
  vtkPolyData* output, vtkDataArray* scaleArray, vtkDataArray* orientArray)
{
  assert(input && output);
  if (input == nullptr || output == nullptr)
  {
    // nothing to do.
    return true;
  }

#if 0
  if (this->GlyphDataRange[0] > this->GlyphDataRange[1])
  {
    vtkErrorMacro(
      "First element in GlyphDataRange must be less than or equal to the second element.");
    return false;
  }
#endif

  if (orientArray && orientArray->GetNumberOfComponents() > 3)
  {
    vtkErrorMacro(<< "vtkDataArray " << orientArray->GetName() << " has more than 3 components.\n");
    return false;
  }

  vtkDebugMacro(<< "Generating glyphs");

  auto pts = vtkSmartPointer<vtkIdList>::New();
  pts->Allocate(VTK_CELL_SIZE);

  unsigned char* inGhostLevels = nullptr;
  vtkDataArray* temp = nullptr;
  auto pd = input->GetPointData();
  if (pd)
  {
    temp = pd->GetArray(vtkDataSetAttributes::GhostArrayName());
  }
  if ((!temp) || (temp->GetDataType() != VTK_UNSIGNED_CHAR) || (temp->GetNumberOfComponents() != 1))
  {
    vtkDebugMacro("No appropriate ghost levels field available.");
  }
  else
  {
    inGhostLevels = static_cast<vtkUnsignedCharArray*>(temp)->GetPointer(0);
  }

  vtkIdType numPts = input->GetNumberOfPoints();
  if (numPts < 1)
  {
    vtkDebugMacro(<< "No points to glyph!");
    return 1;
  }

  // Allocate storage for output PolyData
  vtkPointData* outputPD = output->GetPointData();
  outputPD->CopyVectorsOff();
  outputPD->CopyNormalsOff();
  outputPD->CopyTCoordsOff();

  vtkSmartPointer<vtkPolyData> source = this->GetSource(0, sourceVector);
  if (source == nullptr)
  {
    vtkNew<vtkPolyData> defaultSource;
    defaultSource->Allocate();
    vtkNew<vtkPoints> defaultPoints;
    defaultPoints->Allocate(6);
    defaultPoints->InsertNextPoint(0, 0, 0);
    defaultPoints->InsertNextPoint(1, 0, 0);
    vtkIdType defaultPointIds[2];
    defaultPointIds[0] = 0;
    defaultPointIds[1] = 1;
    defaultSource->SetPoints(defaultPoints);
    defaultSource->InsertNextCell(VTK_LINE, 2, defaultPointIds);
    source = defaultSource;
  }

  auto sourcePts = source->GetPoints();
  vtkIdType numSourcePts = sourcePts->GetNumberOfPoints();
  vtkIdType numSourceCells = source->GetNumberOfCells();

  vtkDataArray* sourceNormals = source->GetPointData()->GetNormals();

  // Prepare to copy output.
  pd = input->GetPointData();
  outputPD->CopyAllocate(pd, numPts * numSourcePts);

  vtkNew<vtkIdList> srcPointIdList;
  srcPointIdList->SetNumberOfIds(numSourcePts);
  vtkNew<vtkIdList> dstPointIdList;
  dstPointIdList->SetNumberOfIds(numSourcePts);
  vtkNew<vtkIdList> srcCellIdList;
  srcCellIdList->SetNumberOfIds(numSourceCells);
  vtkNew<vtkIdList> dstCellIdList;
  dstCellIdList->SetNumberOfIds(numSourceCells);

  auto newPts = vtkSmartPointer<vtkPoints>::New();

  // Set the desired precision for the points in the output.
  if (this->OutputPointsPrecision == vtkAlgorithm::DEFAULT_PRECISION)
  {
    newPts->SetDataType(VTK_FLOAT);
  }
  else if (this->OutputPointsPrecision == vtkAlgorithm::SINGLE_PRECISION)
  {
    newPts->SetDataType(VTK_FLOAT);
  }
  else if (this->OutputPointsPrecision == vtkAlgorithm::DOUBLE_PRECISION)
  {
    newPts->SetDataType(VTK_DOUBLE);
  }

  newPts->Allocate(numPts * numSourcePts);

  vtkSmartPointer<vtkFloatArray> newNormals;
  if (sourceNormals)
  {
    newNormals.TakeReference(vtkFloatArray::New());
    newNormals->SetNumberOfComponents(3);
    newNormals->Allocate(3 * numPts * numSourcePts);
    newNormals->SetName("Normals");
  }

  // Setting up for calls to PolyData::InsertNextCell()
  output->Allocate(source, 3 * numPts * numSourceCells, numPts * numSourceCells);

  vtkSmartPointer<vtkPoints> transformedSourcePts = vtkSmartPointer<vtkPoints>::New();
  transformedSourcePts->SetDataTypeToDouble();
  transformedSourcePts->Allocate(numSourcePts);

  // Traverse all Input points, transforming Source points and copying
  // point attributes.
  auto trans = vtkSmartPointer<vtkTransform>::New();
  vtkNew<vtkIdList> pointIdList;
  vtkIdType ptIncr = 0;
  vtkIdType cellIncr = 0;
  for (vtkIdType inPtId = 0; inPtId < numPts; inPtId++)
  {
    double scalex(1.0), scaley(1.0), scalez(1.0);
    if (!(inPtId % 10000))
    {
      this->UpdateProgress(static_cast<double>(inPtId) / numPts);
      if (this->GetAbortExecute())
      {
        break;
      }
    }

    // Get the scalar and vector data
    if (scaleArray)
    {
      if (scaleArray->GetNumberOfComponents() == 1)
      {
        scalex = scaley = scalez = scaleArray->GetComponent(inPtId, 0);
      }
      else
      {
        // Consider the vector scaling mode
        if (scaleArray->GetNumberOfComponents() == 2)
        {
          double* vec2 = scaleArray->GetTuple(inPtId);
          if (this->VectorScaleMode == SCALE_BY_MAGNITUDE)
          {
            scalex = scaley = scalez = vtkMath::Norm2D(vec2);
          }
          else if (this->VectorScaleMode == SCALE_BY_COMPONENTS)
          {
            scalex = vec2[0];
            scaley = vec2[1];
            // leave scalez alone for 2D
          }
        }
        else if (scaleArray->GetNumberOfComponents() == 3)
        {
          double* vec3 = scaleArray->GetTuple(inPtId);
          if (this->VectorScaleMode == SCALE_BY_MAGNITUDE)
          {
            scalex = scaley = scalez = vtkMath::Norm(vec3);
          }
          else
          {
            scalex = vec3[0];
            scaley = vec3[1];
            scalez = vec3[2];
          }
        }
      }
    }

    // Apply scale factor
    scalex *= this->ScaleFactor;
    scaley *= this->ScaleFactor;
    scalez *= this->ScaleFactor;

    // Check ghost points.
    // If we are processing a piece, we do not want to duplicate
    // glyphs on the borders.
    if (inGhostLevels && inGhostLevels[inPtId] & vtkDataSetAttributes::DUPLICATEPOINT)
    {
      continue;
    }

    // this is used to respect blanking specified on uniform grids.
    vtkUniformGrid* inputUG = vtkUniformGrid::SafeDownCast(input);
    if (inputUG && !inputUG->IsPointVisible(inPtId))
    {
      // input is a vtkUniformGrid and the current point is blanked. Don't glyph
      // it.
      continue;
    }

    if (!this->IsPointVisible(input, inPtId))
    {
      continue;
    }

    // Now begin copying/transforming glyph
    trans->Identity();

    // Copy all topology (transformation independent)
    for (vtkIdType cellId = 0; cellId < numSourceCells; cellId++)
    {
      source->GetCellPoints(cellId, pointIdList);
      int npts = pointIdList->GetNumberOfIds();
      pts->Reset();
      for (vtkIdType i = 0; i < npts; i++)
      {
        pts->InsertId(i, pointIdList->GetId(i) + ptIncr);
      }
      output->InsertNextCell(source->GetCellType(cellId), pts);
    }

    // translate Source to Input point
    double x[3];
    input->GetPoint(inPtId, x);
    trans->Translate(x[0], x[1], x[2]);

    if (orientArray)
    {
      double v[3] = { 0.0 };
      orientArray->GetTuple(inPtId, v);
      double vMag = vtkMath::Norm(v);
      if (vMag > 0.0)
      {
        // if there is no y or z component
        if (v[1] == 0.0 && v[2] == 0.0)
        {
          if (v[0] < 0) // just flip x if we need to
          {
            trans->RotateWXYZ(180.0, 0, 1, 0);
          }
        }
        else
        {
          double vNew[3];
          vNew[0] = (v[0] + vMag) / 2.0;
          vNew[1] = v[1] / 2.0;
          vNew[2] = v[2] / 2.0;
          trans->RotateWXYZ(180.0, vNew[0], vNew[1], vNew[2]);
        }
      }
    }

    // scale data if appropriate
    if (scalex == 0.0)
    {
      scalex = 1.0e-10;
    }
    if (scaley == 0.0)
    {
      scaley = 1.0e-10;
    }
    if (scalez == 0.0)
    {
      scalez = 1.0e-10;
    }
    trans->Scale(scalex, scaley, scalez);

    // multiply points and normals by resulting matrix
    if (this->SourceTransform)
    {
      transformedSourcePts->Reset();
      this->SourceTransform->TransformPoints(sourcePts, transformedSourcePts);
      trans->TransformPoints(transformedSourcePts, newPts);
    }
    else
    {
      trans->TransformPoints(sourcePts, newPts);
    }

    if (newNormals.GetPointer())
    {
      trans->TransformNormals(sourceNormals, newNormals);
    }

    // Copy point data from source (if possible)
    if (pd)
    {
      for (vtkIdType i = 0; i < numSourcePts; ++i)
      {
        srcPointIdList->SetId(i, inPtId);
        dstPointIdList->SetId(i, ptIncr + i);
      }
      outputPD->CopyData(pd, srcPointIdList, dstPointIdList);
    }

    ptIncr += numSourcePts;
    cellIncr += numSourceCells;
  }

  if (newNormals.GetPointer())
  {
    outputPD->SetNormals(newNormals);
  }

  // Update ourselves and release memory
  //
  output->SetPoints(newPts);
  output->Squeeze();

  return true;
}

//-----------------------------------------------------------------------------
void vtkPVGlyphFilter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "GlyphMode: ";
  switch (this->GlyphMode)
  {
    case ALL_POINTS:
      os << "ALL_POINTS" << endl;
      break;

    case EVERY_NTH_POINT:
      os << "EVERY_NTH_POINT" << endl;
      break;

    case SPATIALLY_UNIFORM_DISTRIBUTION:
      os << "SPATIALLY_UNIFORM_DISTRIBUTION" << endl;
      break;

    default:
      os << "(invalid:" << this->GlyphMode << ")" << endl;
  }
  os << indent << "MaximumNumberOfSamplePoints: " << this->MaximumNumberOfSamplePoints << endl;
  os << indent << "Seed: " << this->Seed << endl;
  os << indent << "Stride: " << this->Stride << endl;
  os << indent << "Controller: " << this->Controller << endl;
}
