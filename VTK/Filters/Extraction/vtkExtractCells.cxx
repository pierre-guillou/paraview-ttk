/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkExtractCells.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/*----------------------------------------------------------------------------
 Copyright (c) Sandia Corporation
 See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.
----------------------------------------------------------------------------*/

#include "vtkExtractCells.h"

#include "vtkCell.h"
#include "vtkCellArray.h"
#include "vtkCellData.h"
#include "vtkIdTypeArray.h"
#include "vtkInformation.h"
#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkPointSet.h"
#include "vtkPoints.h"
#include "vtkSMPTools.h"
#include "vtkTimeStamp.h"
#include "vtkUnsignedCharArray.h"
#include "vtkUnstructuredGrid.h"

vtkStandardNewMacro(vtkExtractCells);

#include <algorithm>
#include <numeric>
#include <vector>

namespace
{
struct FastPointMap
{
  using ConstIteratorType = const vtkIdType*;

  vtkNew<vtkIdList> Map;
  vtkIdType LastInput;
  vtkIdType LastOutput;

  ConstIteratorType CBegin() const { return this->Map->GetPointer(0); }

  ConstIteratorType CEnd() const { return this->Map->GetPointer(this->Map->GetNumberOfIds()); }

  vtkIdType* Reset(vtkIdType numValues)
  {
    this->LastInput = -1;
    this->LastOutput = -1;
    this->Map->SetNumberOfIds(numValues);
    return this->Map->GetPointer(0);
  }

  // Map inputId to the new PointId. If inputId is invalid, return -1.
  vtkIdType LookUp(vtkIdType inputId)
  {
    vtkIdType outputId = -1;
    ConstIteratorType first;
    ConstIteratorType last;

    if (this->LastOutput >= 0)
    {
      // Here's the optimization: since the point ids are usually requested
      // with some locality, we can reduce the search range by caching the
      // results of the last lookup. This reduces the number of lookups and
      // improves CPU cache behavior.

      // Offset is the distance (in input space) between the last lookup and
      // the current id. Since the point map is sorted and unique, this is the
      // maximum distance that the current ID can be from the previous one.
      vtkIdType offset = inputId - this->LastInput;

      // Our search range is from the last output location
      first = this->CBegin() + this->LastOutput;
      last = first + offset;

      // Ensure these are correctly ordered (offset may be < 0):
      if (last < first)
      {
        std::swap(first, last);
      }

      // Adjust last to be past-the-end:
      ++last;

      // Clamp to map bounds:
      first = std::max(first, this->CBegin());
      last = std::min(last, this->CEnd());
    }
    else
    { // First run, use full range:
      first = this->CBegin();
      last = this->CEnd();
    }

    outputId = this->BinaryFind(first, last, inputId);
    if (outputId >= 0)
    {
      this->LastInput = inputId;
      this->LastOutput = outputId;
    }

    return outputId;
  }

private:
  // Modified version of std::lower_bound that returns as soon as a value is
  // found (rather than finding the beginning of a sequence). Returns the
  // position in the list, or -1 if not found.
  vtkIdType BinaryFind(ConstIteratorType first, ConstIteratorType last, vtkIdType val) const
  {
    vtkIdType len = last - first;

    while (len > 0)
    {
      // Select median
      vtkIdType half = len / 2;
      ConstIteratorType middle = first + half;

      const vtkIdType& mVal = *middle;
      if (mVal < val)
      { // This soup is too cold.
        first = middle;
        ++first;
        len = len - half - 1;
      }
      else if (val < mVal)
      { // This soup is too hot!
        len = half;
      }
      else
      { // This soup is juuuust right.
        return middle - this->Map->GetPointer(0);
      }
    }

    return -1;
  }
};
} // end anonymous namespace

class vtkExtractCellsSTLCloak
{
public:
  std::vector<vtkIdType> CellIds;
  vtkTimeStamp ModifiedTime;
  vtkTimeStamp SortTime;
  FastPointMap PointMap;

  void Modified() { this->ModifiedTime.Modified(); }

  inline bool IsPrepared() const
  {
    return this->ModifiedTime.GetMTime() < this->SortTime.GetMTime();
  }

  void Prepare(vtkIdType numInputCells)
  {
    if (!this->IsPrepared())
    {
      vtkSMPTools::Sort(this->CellIds.begin(), this->CellIds.end());
      auto last = std::unique(this->CellIds.begin(), this->CellIds.end());
      auto first = this->CellIds.begin();

      // These lines clamp the ids to the number of cells in the
      // dataset. Otherwise segfaults occur when cellIds are greater than the
      // number of input cells, in particular when cellId==numInputCells.
      auto clampLast = std::find(first, last, numInputCells);
      last = (clampLast != last ? clampLast : last);

      this->CellIds.resize(std::distance(first, last));
      this->SortTime.Modified();
    }
  }
};

//----------------------------------------------------------------------------
vtkExtractCells::vtkExtractCells()
{
  this->CellList = new vtkExtractCellsSTLCloak;
}

//----------------------------------------------------------------------------
vtkExtractCells::~vtkExtractCells()
{
  delete this->CellList;
}

//----------------------------------------------------------------------------
void vtkExtractCells::SetCellList(vtkIdList* l)
{
  delete this->CellList;
  this->CellList = new vtkExtractCellsSTLCloak;

  if (l != nullptr)
  {
    this->AddCellList(l);
  }
}

//----------------------------------------------------------------------------
void vtkExtractCells::AddCellList(vtkIdList* l)
{
  const vtkIdType inputSize = l ? l->GetNumberOfIds() : 0;
  if (inputSize == 0)
  {
    return;
  }

  const vtkIdType* inputBegin = l->GetPointer(0);
  const vtkIdType* inputEnd = inputBegin + inputSize;

  const std::size_t oldSize = this->CellList->CellIds.size();
  const std::size_t newSize = oldSize + static_cast<std::size_t>(inputSize);
  this->CellList->CellIds.resize(newSize);

  auto outputBegin = this->CellList->CellIds.begin();
  std::advance(outputBegin, oldSize);

  std::copy(inputBegin, inputEnd, outputBegin);

  this->CellList->Modified();
}

//----------------------------------------------------------------------------
void vtkExtractCells::AddCellRange(vtkIdType from, vtkIdType to)
{
  if (to < from || to < 0)
  {
    vtkWarningMacro("Bad cell range: (" << to << "," << from << ")");
    return;
  }

  // This range specification is inconsistent with C++. Left for backward
  // compatibility reasons.
  const vtkIdType inputSize = to - from + 1; // +1 to include 'to'
  const std::size_t oldSize = this->CellList->CellIds.size();
  const std::size_t newSize = oldSize + static_cast<std::size_t>(inputSize);

  this->CellList->CellIds.resize(newSize);

  auto outputBegin = this->CellList->CellIds.begin() + oldSize;
  auto outputEnd = this->CellList->CellIds.begin() + newSize;

  std::iota(outputBegin, outputEnd, from);

  this->CellList->Modified();
}

//----------------------------------------------------------------------------
vtkMTimeType vtkExtractCells::GetMTime()
{
  vtkMTimeType mTime = this->Superclass::GetMTime();
  mTime = std::max(mTime, this->CellList->ModifiedTime.GetMTime());
  mTime = std::max(mTime, this->CellList->SortTime.GetMTime());
  return mTime;
}

//----------------------------------------------------------------------------
int vtkExtractCells::RequestData(vtkInformation* vtkNotUsed(request),
  vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  // get the input and output
  vtkDataSet* input = vtkDataSet::GetData(inputVector[0]);
  vtkUnstructuredGrid* output = vtkUnstructuredGrid::GetData(outputVector);

  // Sort/uniquify the cell ids if needed.
  vtkIdType numCellsInput = input->GetNumberOfCells();
  this->CellList->Prepare(numCellsInput);

  this->InputIsUgrid = ((vtkUnstructuredGrid::SafeDownCast(input)) != nullptr);

  vtkIdType numCells = static_cast<vtkIdType>(this->CellList->CellIds.size());

  if (numCells == numCellsInput)
  {
    this->Copy(input, output);
    return 1;
  }

  vtkPointData* inPD = input->GetPointData();
  vtkCellData* inCD = input->GetCellData();

  if (numCells == 0)
  {
    // set up a ugrid with same data arrays as input, but
    // no points, cells or data.
    output->Allocate(1);
    output->GetPointData()->CopyGlobalIdsOn();
    output->GetPointData()->CopyAllocate(inPD, VTK_CELL_SIZE);
    output->GetCellData()->CopyGlobalIdsOn();
    output->GetCellData()->CopyAllocate(inCD, 1);

    vtkNew<vtkPoints> pts;
    pts->SetNumberOfPoints(0);
    output->SetPoints(pts);
    return 1;
  }

  vtkPointData* newPD = output->GetPointData();
  vtkCellData* newCD = output->GetCellData();

  vtkIdType numPoints = this->ReMapPointIds(input);

  newPD->CopyGlobalIdsOn();
  newPD->CopyAllocate(inPD, numPoints);

  newCD->CopyGlobalIdsOn();
  newCD->CopyAllocate(inCD, numCells);

  vtkNew<vtkPoints> pts;
  if (vtkPointSet* inputPS = vtkPointSet::SafeDownCast(input))
  {
    // preserve input datatype
    pts->SetDataType(inputPS->GetPoints()->GetDataType());
  }
  pts->SetNumberOfPoints(numPoints);
  output->SetPoints(pts);

  // Copy points and point data:
  vtkPointSet* pointSet = vtkPointSet::SafeDownCast(input);
  if (pointSet)
  {
    // Optimize when a vtkPoints object exists in the input:
    vtkNew<vtkIdList> dstIds; // contiguous range [0, numPoints)
    dstIds->SetNumberOfIds(numPoints);
    std::iota(dstIds->GetPointer(0), dstIds->GetPointer(numPoints), 0);

    pts->InsertPoints(dstIds, this->CellList->PointMap.Map, pointSet->GetPoints());
    newPD->CopyData(inPD, this->CellList->PointMap.Map, dstIds);
  }
  else
  {
    // Slow path if we have to query the dataset:
    for (vtkIdType newId = 0; newId < numPoints; ++newId)
    {
      vtkIdType oldId = this->CellList->PointMap.Map->GetId(newId);
      pts->SetPoint(newId, input->GetPoint(oldId));
      newPD->CopyData(inPD, oldId, newId);
    }
  }

  if (this->InputIsUgrid)
  {
    this->CopyCellsUnstructuredGrid(input, output);
  }
  else
  {
    this->CopyCellsDataSet(input, output);
  }

  this->CellList->PointMap.Reset(0);
  output->Squeeze();

  return 1;
}

//----------------------------------------------------------------------------
void vtkExtractCells::Copy(vtkDataSet* input, vtkUnstructuredGrid* output)
{
  // If input is unstructured grid just deep copy through
  if (this->InputIsUgrid)
  {
    output->DeepCopy(vtkUnstructuredGrid::SafeDownCast(input));
    return;
  }

  vtkIdType numPoints = input->GetNumberOfPoints();
  vtkIdType numCells = input->GetNumberOfCells();

  vtkPointData* inPD = input->GetPointData();
  vtkCellData* inCD = input->GetCellData();
  vtkPointData* newPD = output->GetPointData();
  vtkCellData* newCD = output->GetCellData();
  newPD->CopyAllocate(inPD, numPoints);
  newCD->CopyAllocate(inCD, numCells);

  output->Allocate(numCells);

  vtkNew<vtkPoints> pts;
  pts->SetNumberOfPoints(numPoints);
  output->SetPoints(pts);

  for (vtkIdType i = 0; i < numPoints; i++)
  {
    pts->SetPoint(i, input->GetPoint(i));
  }
  newPD->DeepCopy(inPD);

  vtkNew<vtkIdList> cellPoints;
  for (vtkIdType cellId = 0; cellId < numCells; cellId++)
  {
    input->GetCellPoints(cellId, cellPoints);
    output->InsertNextCell(input->GetCellType(cellId), cellPoints);
  }
  newCD->DeepCopy(inCD);

  output->Squeeze();
}

//----------------------------------------------------------------------------
vtkIdType vtkExtractCells::ReMapPointIds(vtkDataSet* grid)
{
  vtkIdType totalPoints = grid->GetNumberOfPoints();
  std::vector<char> temp(totalPoints, 0);

  vtkIdType numberOfIds = 0;

  if (!this->InputIsUgrid)
  {
    vtkNew<vtkIdList> ptIds;

    for (vtkIdType cellId : this->CellList->CellIds)
    {
      grid->GetCellPoints(cellId, ptIds);

      vtkIdType npts = ptIds->GetNumberOfIds();
      vtkIdType* pts = ptIds->GetPointer(0);

      for (vtkIdType i = 0; i < npts; ++i)
      {
        vtkIdType pid = pts[i];
        if (temp[pid] == 0)
        {
          ++numberOfIds;
          temp[pid] = 1;
        }
      }
    }
  }
  else
  {
    vtkUnstructuredGrid* ugrid = vtkUnstructuredGrid::SafeDownCast(grid);
    vtkIdType maxid = ugrid->GetNumberOfCells();

    this->SubSetUGridCellArraySize = 0;
    this->SubSetUGridFacesArraySize = 0;

    for (vtkIdType cellId : this->CellList->CellIds)
    {
      if (cellId > maxid)
      {
        continue;
      }

      vtkIdType npts, *pts;
      ugrid->GetCellPoints(cellId, npts, pts);

      this->SubSetUGridCellArraySize += (1 + npts);

      for (vtkIdType i = 0; i < npts; ++i)
      {
        vtkIdType pid = pts[i];
        if (temp[pid] == 0)
        {
          ++numberOfIds;
          temp[pid] = 1;
        }
      }

      if (ugrid->GetCellType(cellId) == VTK_POLYHEDRON)
      {
        vtkIdType nfaces, *ptids;
        ugrid->GetFaceStream(cellId, nfaces, ptids);
        this->SubSetUGridFacesArraySize += 1;
        for (vtkIdType j = 0; j < nfaces; j++)
        {
          vtkIdType nfpts = *ptids;
          this->SubSetUGridFacesArraySize += nfpts + 1;
          ptids += nfpts + 1;
        }
      }
    }
  }

  vtkIdType* pointMap = this->CellList->PointMap.Reset(numberOfIds);
  for (vtkIdType pid = 0; pid < totalPoints; pid++)
  {
    if (temp[pid])
    {
      (*pointMap++) = pid;
    }
  }

  return numberOfIds;
}

//----------------------------------------------------------------------------
void vtkExtractCells::CopyCellsDataSet(vtkDataSet* input, vtkUnstructuredGrid* output)
{
  output->Allocate(static_cast<vtkIdType>(this->CellList->CellIds.size()));

  vtkCellData* oldCD = input->GetCellData();
  vtkCellData* newCD = output->GetCellData();

  // We only create vtkOriginalCellIds for the output data set if it does not
  // exist in the input data set. If it is in the input data set then we
  // let CopyData() take care of copying it over.
  vtkIdTypeArray* origMap = nullptr;
  if (oldCD->GetArray("vtkOriginalCellIds") == nullptr)
  {
    origMap = vtkIdTypeArray::New();
    origMap->SetNumberOfComponents(1);
    origMap->SetName("vtkOriginalCellIds");
    newCD->AddArray(origMap);
    origMap->Delete();
  }

  vtkNew<vtkIdList> cellPoints;

  for (vtkIdType cellId : this->CellList->CellIds)
  {
    input->GetCellPoints(cellId, cellPoints);

    for (vtkIdType i = 0; i < cellPoints->GetNumberOfIds(); i++)
    {
      vtkIdType oldId = cellPoints->GetId(i);

      vtkIdType newId = this->CellList->PointMap.LookUp(oldId);
      assert("Old id exists in map." && newId >= 0);

      cellPoints->SetId(i, newId);
    }
    vtkIdType newId = output->InsertNextCell(input->GetCellType(cellId), cellPoints);

    newCD->CopyData(oldCD, cellId, newId);
    if (origMap)
    {
      origMap->InsertNextValue(cellId);
    }
  }
}

//----------------------------------------------------------------------------
void vtkExtractCells::CopyCellsUnstructuredGrid(vtkDataSet* input, vtkUnstructuredGrid* output)
{
  vtkUnstructuredGrid* ugrid = vtkUnstructuredGrid::SafeDownCast(input);
  if (ugrid == nullptr)
  {
    this->CopyCellsDataSet(input, output);
    return;
  }

  vtkCellData* oldCD = input->GetCellData();
  vtkCellData* newCD = output->GetCellData();

  // We only create vtkOriginalCellIds for the output data set if it does not
  // exist in the input data set. If it is in the input data set then we
  // let CopyData() take care of copying it over.
  vtkIdTypeArray* origMap = nullptr;
  if (oldCD->GetArray("vtkOriginalCellIds") == nullptr)
  {
    origMap = vtkIdTypeArray::New();
    origMap->SetNumberOfComponents(1);
    origMap->SetName("vtkOriginalCellIds");
    newCD->AddArray(origMap);
    origMap->Delete();
  }

  vtkIdType numCells = static_cast<vtkIdType>(this->CellList->CellIds.size());

  vtkNew<vtkCellArray> cellArray; // output
  vtkNew<vtkIdTypeArray> newcells;
  newcells->SetNumberOfValues(this->SubSetUGridCellArraySize);
  cellArray->SetCells(numCells, newcells);
  vtkIdType cellArrayIdx = 0;

  vtkNew<vtkIdTypeArray> locationArray;
  locationArray->SetNumberOfValues(numCells);
  vtkNew<vtkIdTypeArray> facesLocationArray;
  facesLocationArray->SetNumberOfValues(numCells);
  vtkNew<vtkIdTypeArray> facesArray;
  facesArray->SetNumberOfValues(this->SubSetUGridFacesArraySize);
  vtkNew<vtkUnsignedCharArray> typeArray;
  typeArray->SetNumberOfValues(numCells);

  vtkIdType nextCellId = 0;
  vtkIdType nextFaceId = 0;

  vtkIdType maxid = ugrid->GetNumberOfCells();
  bool havePolyhedron = false;

  for (vtkIdType oldCellId : this->CellList->CellIds)
  {
    if (oldCellId >= maxid)
    {
      continue;
    }

    unsigned char cellType = ugrid->GetCellType(oldCellId);
    typeArray->SetValue(nextCellId, cellType);

    locationArray->SetValue(nextCellId, cellArrayIdx);

    vtkIdType npts, *pts;
    ugrid->GetCellPoints(oldCellId, npts, pts);

    newcells->SetValue(cellArrayIdx++, npts);

    for (vtkIdType i = 0; i < npts; i++)
    {
      vtkIdType oldId = pts[i];
      vtkIdType newId = this->CellList->PointMap.LookUp(oldId);
      assert("Old id exists in map." && newId >= 0);
      newcells->SetValue(cellArrayIdx++, newId);
    }

    if (cellType == VTK_POLYHEDRON)
    {
      havePolyhedron = true;
      vtkIdType nfaces, *ptids;
      ugrid->GetFaceStream(oldCellId, nfaces, ptids);

      facesLocationArray->SetValue(nextCellId, nextFaceId);
      facesArray->SetValue(nextFaceId++, nfaces);

      for (vtkIdType j = 0; j < nfaces; j++)
      {
        vtkIdType nfpts = *ptids++;
        facesArray->SetValue(nextFaceId++, nfpts);
        for (vtkIdType i = 0; i < nfpts; i++)
        {
          vtkIdType oldId = *ptids++;
          vtkIdType newId = this->CellList->PointMap.LookUp(oldId);
          assert("Old id exists in map." && newId >= 0);
          facesArray->SetValue(nextFaceId++, newId);
        }
      }
    }
    else
    {
      facesLocationArray->SetValue(nextCellId, -1);
    }

    newCD->CopyData(oldCD, oldCellId, nextCellId);
    if (origMap)
    {
      origMap->InsertNextValue(oldCellId);
    }
    nextCellId++;
  }

  if (havePolyhedron)
  {
    output->SetCells(typeArray, locationArray, cellArray, facesLocationArray, facesArray);
  }
  else
  {
    output->SetCells(typeArray, locationArray, cellArray, nullptr, nullptr);
  }
}

//----------------------------------------------------------------------------
int vtkExtractCells::FillInputPortInformation(int, vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
  return 1;
}

//----------------------------------------------------------------------------
void vtkExtractCells::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
