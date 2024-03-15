// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

#include "vtkCellGrid.h"
#include "vtkDGEdge.h"
#include "vtkDGHex.h"
#include "vtkDGQuad.h"
#include "vtkDGTet.h"
#include "vtkDGTri.h"
#include "vtkDGVert.h"
#include "vtkFiltersCellGrid.h"
#include "vtkNew.h"
#include "vtkSmartPointer.h"
#include "vtkTypeFloat32Array.h"
#include "vtkTypeInt32Array.h"

namespace
{

template <typename CellType>
bool TestDGCellType()
{
  vtkNew<vtkCellGrid> grid;
  auto cell = vtkCellMetadata::NewInstance<CellType>(grid);
  if (!cell)
  {
    return false;
  }

  std::cout << "Created " << cell->GetClassName() << " metadata:\n";

  if (cell->GetNumberOfCells() != 0)
  {
    std::cerr << "ERROR: Expected 0 cells present, found " << cell->GetNumberOfCells() << ".\n";
    return false;
  }

  auto refPts = cell->GetReferencePoints();
  if (!refPts || refPts->GetNumberOfTuples() != cell->GetNumberOfCorners())
  {
    std::cerr << "ERROR: Expected " << cell->GetNumberOfCorners() << ", got "
              << (refPts ? refPts->GetNumberOfTuples() : -1) << "\n";
    return false;
  }
  if (cell->GetNumberOfCorners() != vtkDGCell::GetShapeCornerCount(cell->GetShape()))
  {
    std::cerr << "ERROR: Mismatched corner counts " << cell->GetNumberOfCorners() << " vs. "
              << vtkDGCell::GetShapeCornerCount(cell->GetShape()) << "\n";
    return false;
  }
  std::cout << "  " << refPts->GetNumberOfTuples() << " reference points:\n";
  for (vtkIdType ii = 0; ii < refPts->GetNumberOfTuples(); ++ii)
  {
    std::array<double, 3> coords;
    refPts->GetTuple(ii, coords.data());
    std::cout << "    " << ii << ": " << coords[0] << " " << coords[1] << " " << coords[2] << "\n";
    if (coords != cell->GetCornerParameter(ii))
    {
      std::cerr << "ERROR: Bad reference point " << ii << "\n";
      return false;
    }
  }

  auto sideConn = cell->GetSideConnectivity();
  if (!sideConn)
  {
    std::cerr << "ERROR: Expected non-null side connectivity.\n";
    return false;
  }

  bool haveSelfSide = (CellType::Dimension < 3);
  auto sideOffs = cell->GetSideOffsetsAndShapes();
  if (!sideOffs)
  {
    std::cerr << "ERROR: Expected non-null side offsets and shapes.\n";
    return false;
  }
  std::cout << "  " << sideOffs->GetNumberOfTuples() << " side types ("
            << (haveSelfSide ? "including" : "excluding") << " self):\n";

  // Test that side -1 returns the entire cell's connectivity.
  auto cellConn = cell->GetSideConnectivity(-1);
  if (cellConn.size() != static_cast<std::size_t>(cell->GetNumberOfCorners()))
  {
    std::cerr << "ERROR: Bad connectivity for side -1 (expected " << cell->GetNumberOfCorners()
              << " entries, got " << cellConn.size() << ").\n";
    return false;
  }
  std::cout << "    -1. " << vtkDGCell::GetShapeName(cell->GetShape()).Data() << " \"side\":\n";
  vtkIdType expectedNode = 0;
  for (const auto& nodeId : cellConn)
  {
    std::cout << "      " << expectedNode << ": " << nodeId << "\n";
    if (nodeId != expectedNode)
    {
      std::cerr << "\nERROR: Bad connectivity entry " << expectedNode << " in side -1: " << nodeId
                << "\n";
      return false;
    }
    ++expectedNode;
  }

  // Now test that "positive" sides match values in the sides+offsets array.
  // Note that for cells of dimension 2 or less, the input cell's connectivity
  // is reported as the first side in the sideOffs/sideConn arrays so that
  // these cells can be rendered directly. We must account for that by
  // offsetting ss and sideDim below.
  int ss = haveSelfSide ? -1 : 0;
  for (vtkIdType ii = 0; ii < sideOffs->GetNumberOfTuples() - 1; ++ii)
  {
    int offset = sideOffs->GetTuple(ii)[0];
    auto shape = static_cast<vtkDGCell::Shape>(sideOffs->GetTuple(ii)[1]);
    // clang-format off
    std::cout
      << "    " << (ii + (haveSelfSide ? 0 : 1)) << ". "
      << vtkDGCell::GetShapeName(shape).Data() << " sides (@ " << offset << ")\n";
    // clang-format on
    int nn = vtkDGCell::GetShapeCornerCount(shape);
    int nextOffset = sideOffs->GetTuple(ii + 1)[0];
    int sideDim = cell->GetDimension() - (haveSelfSide ? ii : ii + 1);
    int numSidesOfType = ss < 0 ? 1 : cell->GetNumberOfSidesOfDimension(sideDim);
    if (nextOffset - offset != nn * numSidesOfType)
    {
      std::cerr << "ERROR: Bad offset " << offset << " to " << nextOffset << " vs "
                << (nn * numSidesOfType) << "\n";
      return false;
    }
    for (int jj = 0; jj < numSidesOfType; ++jj, ++ss)
    {
      std::cout << "      " << ss << ":";
      for (int kk = 0; kk < nn; ++kk)
      {
        std::cout << " " << sideConn->GetTuple1(offset + jj * nn + kk);
        if (sideConn->GetTuple1(offset + jj * nn + kk) != cell->GetSideConnectivity(ss)[kk])
        {
          std::cerr << "\nERROR: Bad point ID @ kk = " << kk << "\n";
          return false;
        }
      }
      std::cout << "\n";
    }
  }

  std::cout << "  … passed\n\n";
  return true;
}

}

int TestDGCells(int vtkNotUsed(argc), char* vtkNotUsed(argv)[])
{
  vtkFiltersCellGrid::RegisterCellsAndResponders();
  // clang-format off
  if (
    !TestDGCellType<vtkDGEdge>() ||
    !TestDGCellType<vtkDGHex>() ||
    !TestDGCellType<vtkDGQuad>() ||
    !TestDGCellType<vtkDGTet>() ||
    !TestDGCellType<vtkDGTri>() ||
    !TestDGCellType<vtkDGVert>())
  {
    return EXIT_FAILURE;
  }
  // clang-format on

  return EXIT_SUCCESS;
}
