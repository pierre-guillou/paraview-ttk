// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkHyperTreeGridThreshold.h"

#include "vtkArrayDispatch.h"
#include "vtkBitArray.h"
#include "vtkCellData.h"
#include "vtkDataArrayRange.h"
#include "vtkHyperTree.h"
#include "vtkHyperTreeGrid.h"
#include "vtkIdTypeArray.h"
#include "vtkIndexedArray.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMath.h"
#include "vtkObjectFactory.h"
#include "vtkUniformHyperTreeGrid.h"

#include "vtkHyperTreeGridNonOrientedCursor.h"

#include <cmath>
#include <limits>

namespace
{

/*
 * Pure abstract interface for implementing how to deal with output
 * cell data during the thresholding
 */
struct CellDataManager
{
public:
  CellDataManager(vtkCellData* inputData, vtkCellData* outputData)
    : InputData(inputData)
    , OutputData(outputData)
  {
  }

  virtual ~CellDataManager() = default;

  virtual void operator()(vtkIdType inputIndex, vtkIdType outputIndex) = 0;

  virtual void WrapUp() = 0;

protected:
  vtkCellData* InputData = nullptr;
  vtkCellData* OutputData = nullptr;
};

/*
 * Cell data management implementation for the DeepThreshold strategy.
 * Implements a copy of the input data into the output data.
 */
struct CellDataCopier : public CellDataManager
{
public:
  CellDataCopier(vtkCellData* inputData, vtkCellData* outputData)
    : CellDataManager(inputData, outputData)
  {
    this->OutputData->CopyAllocate(this->InputData);
  }

  ~CellDataCopier() override = default;

  void operator()(vtkIdType inputIndex, vtkIdType outputIndex) override
  {
    this->OutputData->CopyData(this->InputData, inputIndex, outputIndex);
  }

  void WrapUp() override { this->OutputData->Squeeze(); }
};

/*
 * Utility struct for dispatching input arrays and creating
 * the corresponding output vtkIndexedArrays.
 */
struct IndexedArrayInitializer
{
public:
  IndexedArrayInitializer(vtkIdTypeArray* handles, vtkCellData* output)
    : Handles(handles)
    , Output(output)
  {
  }

  template <class ArrayT>
  void operator()(ArrayT* input)
  {
    using ValueType = vtk::GetAPIType<ArrayT>;
    vtkNew<vtkIndexedArray<ValueType>> indexed;
    indexed->SetName(input->GetName());
    indexed->SetNumberOfComponents(input->GetNumberOfComponents());
    indexed->ConstructBackend(this->Handles, input);
    this->Output->AddArray(indexed);
  }

private:
  vtkIdTypeArray* Handles = nullptr;
  vtkCellData* Output = nullptr;
};

/*
 * Cell data management implementation for the CopyStructureAndIndexArrays strategy.
 * Implements an indexation of input cell data in the output using vtkIndexedArrays
 * and a shared index mapping.
 */
struct CellDataIndexer : public CellDataManager
{
public:
  CellDataIndexer(vtkCellData* inputData, vtkCellData* outputData)
    : CellDataManager(inputData, outputData)
    , IndirectionMap(vtkSmartPointer<vtkIdTypeArray>::New())
  {
    this->OutputData->CopyAllocate(this->InputData, 1, 1);
    this->IndirectionMap->SetNumberOfComponents(1);
    this->IndirectionMap->SetNumberOfTuples(0);
    using SupportedArrays = vtkArrayDispatch::Arrays;
    using Dispatcher = vtkArrayDispatch::DispatchByArray<SupportedArrays>;
    for (vtkIdType iArr = 0; iArr < this->InputData->GetNumberOfArrays(); ++iArr)
    {
      auto inputArr = this->InputData->GetArray(iArr);
      if (!inputArr)
      {
        // skip all arrays that are not data arrays
        continue;
      }
      IndexedArrayInitializer initializer(this->IndirectionMap, this->OutputData);
      if (!Dispatcher::Execute(inputArr, initializer))
      {
        initializer(inputArr);
      }
    }
  }

  void operator()(vtkIdType inputIndex, vtkIdType outputIndex) override
  {
    this->IndirectionMap->InsertValue(outputIndex, inputIndex);
  }

  void WrapUp() override
  {
    for (vtkIdType iArr = 0; iArr < this->OutputData->GetNumberOfArrays(); ++iArr)
    {
      auto arr = this->OutputData->GetArray(iArr);
      if (!arr)
      {
        // skip all arrays that are not data arrays
        continue;
      }
      arr->SetNumberOfTuples(this->IndirectionMap->GetNumberOfTuples());
    }
  }

private:
  vtkSmartPointer<vtkIdTypeArray> IndirectionMap;
};

}

VTK_ABI_NAMESPACE_BEGIN
//------------------------------------------------------------------------------
struct vtkHyperTreeGridThreshold::Internals
{
  std::unique_ptr<::CellDataManager> CDManager;
};

//------------------------------------------------------------------------------
vtkStandardNewMacro(vtkHyperTreeGridThreshold);

//------------------------------------------------------------------------------
vtkHyperTreeGridThreshold::vtkHyperTreeGridThreshold()
  : Internal(new Internals)
{
  // Use minimum double value by default for lower threshold bound
  this->LowerThreshold = std::numeric_limits<double>::min();

  // Use maximum double value by default for upper threshold bound
  this->UpperThreshold = std::numeric_limits<double>::max();

  // This filter always creates an output with a material mask
  // JBL Ce n'est que dans de tres rares cas que le mask produit par le
  // JBL threshold, que ce soit avec ou sans creation d'un nouveau maillage,
  // JBL ne contienne que des valeurs a false. Ce n'est que dans ces
  // JBL tres rares cas que la creation d'un mask n'aurait pas d'utilite.
  this->OutMask = vtkBitArray::New();

  // Output indices begin at 0
  this->CurrentId = 0;

  // Process active point scalars by default
  this->SetInputArrayToProcess(
    0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS_THEN_CELLS, vtkDataSetAttributes::SCALARS);

  // Input scalars point to null by default
  this->InScalars = nullptr;

  // JB Pour sortir un maillage de meme type que celui en entree, si create
  this->AppropriateOutput = true;
}

//------------------------------------------------------------------------------
vtkHyperTreeGridThreshold::~vtkHyperTreeGridThreshold()
{
  this->OutMask->Delete();
  this->OutMask = nullptr;
}

//------------------------------------------------------------------------------
void vtkHyperTreeGridThreshold::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  os << indent << "LowerThreshold: " << this->LowerThreshold << endl;
  os << indent << "UpperThreshold: " << this->UpperThreshold << endl;
  os << indent << "OutMask: " << this->OutMask << endl;
  os << indent << "CurrentId: " << this->CurrentId << endl;

  if (this->InScalars)
  {
    os << indent << "InScalars:\n";
    this->InScalars->PrintSelf(os, indent.GetNextIndent());
  }
  else
  {
    os << indent << "InScalars: (none)\n";
  }

  os << indent << "MemoryStrategy: " << this->MemoryStrategy << std::endl;
}

//------------------------------------------------------------------------------
int vtkHyperTreeGridThreshold::FillOutputPortInformation(int, vtkInformation* info)
{
  info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkHyperTreeGrid");
  return 1;
}

//------------------------------------------------------------------------------
void vtkHyperTreeGridThreshold::ThresholdBetween(double minimum, double maximum)
{
  this->LowerThreshold = minimum;
  this->UpperThreshold = maximum;
  this->Modified();
}

//------------------------------------------------------------------------------
int vtkHyperTreeGridThreshold::ProcessTrees(vtkHyperTreeGrid* input, vtkDataObject* outputDO)
{
  // Downcast output data object to hyper tree grid
  vtkHyperTreeGrid* output = vtkHyperTreeGrid::SafeDownCast(outputDO);
  if (!output)
  {
    vtkErrorMacro("Incorrect type of output: " << outputDO->GetClassName());
    return 0;
  }

  // Retrieve scalar quantity of interest
  this->InScalars = this->GetInputArrayToProcess(0, input);
  if (!this->InScalars)
  {
    vtkWarningMacro(<< "No scalar data to threshold");
    return 1;
  }

  // JBL Pour les cas extremes ou le filtre est insere dans une chaine
  // JBL de traitement, on pourrait ajouter ici un controle optionnel
  // JBL afin de voir entre le datarange de inscalars et
  // JBL l'interval [LowerThreshold, UpperThreshold] il y a :
  // JBL - un total recouvrement, alors output est le input
  // JBL - pasde recouvrement, alors output est un maillage vide.

  // Retrieve material mask
  this->InMask = input->HasMask() ? input->GetMask() : nullptr;

  if (this->MemoryStrategy == MaskInput)
  {
    output->ShallowCopy(input);

    this->OutMask->SetNumberOfTuples(output->GetNumberOfCells());

    // Iterate over all input and output hyper trees
    vtkIdType outIndex;
    vtkHyperTreeGrid::vtkHyperTreeGridIterator it;
    output->InitializeTreeIterator(it);
    vtkNew<vtkHyperTreeGridNonOrientedCursor> outCursor;
    while (it.GetNextTree(outIndex))
    {
      if (this->CheckAbort())
      {
        break;
      }
      // Initialize new grid cursor at root of current input tree
      output->InitializeNonOrientedCursor(outCursor, outIndex);
      // Limit depth recursively
      this->RecursivelyProcessTreeWithCreateNewMask(outCursor);
    } // it
  }
  else if (this->MemoryStrategy == CopyStructureAndIndexArrays ||
    this->MemoryStrategy == DeepThreshold)
  {
    // Set grid parameters
    output->SetDimensions(input->GetDimensions());
    output->SetTransposedRootIndexing(input->GetTransposedRootIndexing());
    output->SetBranchFactor(input->GetBranchFactor());
    output->CopyCoordinates(input);
    output->SetHasInterface(input->GetHasInterface());
    output->SetInterfaceNormalsName(input->GetInterfaceNormalsName());
    output->SetInterfaceInterceptsName(input->GetInterfaceInterceptsName());

    // Initialize cell data manager
    switch (this->MemoryStrategy)
    {
      // MaskInput is handled above
      case CopyStructureAndIndexArrays:
        this->Internal->CDManager = std::unique_ptr<::CellDataManager>(
          new ::CellDataIndexer(input->GetCellData(), output->GetCellData()));
        break;
      case DeepThreshold:
        this->Internal->CDManager = std::unique_ptr<::CellDataManager>(
          new ::CellDataCopier(input->GetCellData(), output->GetCellData()));
        break;
      default:
        this->Internal->CDManager = std::unique_ptr<::CellDataManager>(
          new ::CellDataCopier(input->GetCellData(), output->GetCellData()));
        vtkWarningMacro("No switch case for given MemoryStrategy "
          << this->MemoryStrategy << " defaulting to DeepThreshold");
        break;
    }

    // Output indices begin at 0
    this->CurrentId = 0;

    // Iterate over all input and output hyper trees
    vtkIdType inIndex;
    vtkHyperTreeGrid::vtkHyperTreeGridIterator it;
    input->InitializeTreeIterator(it);
    vtkNew<vtkHyperTreeGridNonOrientedCursor> inCursor;
    vtkNew<vtkHyperTreeGridNonOrientedCursor> outCursor;
    while (it.GetNextTree(inIndex))
    {
      if (this->CheckAbort())
      {
        break;
      }
      // Initialize new cursor at root of current input tree
      input->InitializeNonOrientedCursor(inCursor, inIndex);
      // Initialize new cursor at root of current output tree
      output->InitializeNonOrientedCursor(outCursor, inIndex, true);
      // Limit depth recursively
      this->RecursivelyProcessTree(inCursor, outCursor);
    } // it

    this->Internal->CDManager->WrapUp();
  }
  else
  {
    vtkErrorMacro(
      "No corresponding MemoryStrategyChoice for MemoryStrategy = " << this->MemoryStrategy);
    return 0;
  }

  // Squeeze and set output material mask if necessary
  this->OutMask->Squeeze();
  output->SetMask(this->OutMask);

  this->UpdateProgress(1.);
  return 1;
}

//------------------------------------------------------------------------------
bool vtkHyperTreeGridThreshold::RecursivelyProcessTree(
  vtkHyperTreeGridNonOrientedCursor* inCursor, vtkHyperTreeGridNonOrientedCursor* outCursor)
{
  // Retrieve global index of input cursor
  vtkIdType inId = inCursor->GetGlobalNodeIndex();

  // Increase index count on output: postfix is intended
  vtkIdType outId = this->CurrentId++;

  // Copy out cell data from that of input cell
  if (!this->Internal->CDManager)
  {
    vtkErrorMacro("Must set the CellDataManager before processing trees");
    return false;
  }
  (*(this->Internal->CDManager))(inId, outId);

  // Retrieve output tree and set global index of output cursor
  vtkHyperTree* outTree = outCursor->GetTree();
  outTree->SetGlobalIndexFromLocal(outCursor->GetVertexId(), outId);

  // Flag to recursively decide whether a tree node should discarded
  bool discard = true;

  if (this->InMask && this->InMask->GetValue(inId))
  {
    // Mask output cell if necessary
    this->OutMask->InsertTuple1(outId, discard);

    // Return whether current node is within range
    return discard;
  }

  // Descend further into input trees only if cursor is not at leaf
  if (!inCursor->IsLeaf())
  {
    // Cursor is not at leaf, subdivide output tree one level further
    outCursor->SubdivideLeaf();

    // If input cursor is neither at leaf nor at maximum depth, recurse to all children
    int numChildren = inCursor->GetNumberOfChildren();
    for (int ichild = 0; ichild < numChildren; ++ichild)
    {
      if (this->CheckAbort())
      {
        break;
      }
      // Descend into child in input grid as well
      inCursor->ToChild(ichild);
      // Descend into child in output grid as well
      outCursor->ToChild(ichild);
      // Recurse and keep track of whether some children are kept
      discard &= this->RecursivelyProcessTree(inCursor, outCursor);
      // Return to parent in input grid
      outCursor->ToParent();
      // Return to parent in output grid
      inCursor->ToParent();
    } // child
  }   // if (! inCursor->IsLeaf() && inCursor->GetCurrentDepth() < this->Depth)
  else
  {
    // Input cursor is at leaf, check whether it is within range
    double value = this->InScalars->GetTuple1(inId);
    if (!(this->InMask && this->InMask->GetValue(inId)) && value >= this->LowerThreshold &&
      value <= this->UpperThreshold)
    {
      // Cell is not masked and is within range, keep it
      discard = false;
    }
  } // else

  // Mask output cell if necessary
  this->OutMask->InsertTuple1(outId, discard);

  // Return whether current node is within range
  return discard;
}

//------------------------------------------------------------------------------
bool vtkHyperTreeGridThreshold::RecursivelyProcessTreeWithCreateNewMask(
  vtkHyperTreeGridNonOrientedCursor* outCursor)
{
  // Retrieve global index of input cursor
  vtkIdType outId = outCursor->GetGlobalNodeIndex();

  // Flag to recursively decide whether a tree node should discarded
  bool discard = true;

  if (this->InMask && this->InMask->GetValue(outId))
  {
    // Mask output cell if necessary
    this->OutMask->InsertTuple1(outId, discard);

    // Return whether current node is within range
    return discard;
  }

  // Descend further into input trees only if cursor is not at leaf
  if (!outCursor->IsLeaf())
  {
    // If input cursor is neither at leaf nor at maximum depth, recurse to all children
    int numChildren = outCursor->GetNumberOfChildren();
    for (int ichild = 0; ichild < numChildren; ++ichild)
    {
      if (this->CheckAbort())
      {
        break;
      }
      // Descend into child in output grid as well
      outCursor->ToChild(ichild);
      // Recurse and keep track of whether some children are kept
      discard &= this->RecursivelyProcessTreeWithCreateNewMask(outCursor);
      // Return to parent in output grid
      outCursor->ToParent();
    } // child
  }   // if (! inCursor->IsLeaf() && inCursor->GetCurrentDepth() < this->Depth)
  else
  {
    // Input cursor is at leaf, check whether it is within range
    double value = this->InScalars->GetTuple1(outId);
    discard = value < this->LowerThreshold || value > this->UpperThreshold;
  } // else

  // Mask output cell if necessary
  this->OutMask->InsertTuple1(outId, discard);

  // Return whether current node is within range
  return discard;
}
VTK_ABI_NAMESPACE_END
