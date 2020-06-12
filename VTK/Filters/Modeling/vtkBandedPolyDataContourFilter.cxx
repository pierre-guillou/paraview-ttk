/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkBandedPolyDataContourFilter.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkBandedPolyDataContourFilter.h"

#include <algorithm>
#include <iterator>
#include <numeric>
#include <vector>

#include "vtkCellArray.h"
#include "vtkCellData.h"
#include "vtkDoubleArray.h"
#include "vtkEdgeTable.h"
#include "vtkExecutive.h"
#include "vtkFloatArray.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkPolyData.h"
#include "vtkTriangleStrip.h"

#include <cfloat>

vtkStandardNewMacro(vtkBandedPolyDataContourFilter);

namespace
{
// Symbolic value for recording that no intersection points were generated by
// ClipEdge. vtkEdgeTable::IsEdge uses -1 to indicate that the edge is not
// stored, so we need a different value is not a valid cell index to indicate
// that an edge has no intersection points.
constexpr vtkIdType NO_INTERSECTION = -999;

//------------------------------------------------------------------------------
// Bookkeeping of polygon points
enum class PointType
{
  VERTEX = 0,      // a point of the original cell with a scalar value
                   // NOT equal to a clip value
  CLIP_VERTEX = 1, // a point of the original cell with a scalar value
                   // equal to a clip value
  EDGE = 2         // a point on the edge of the original cell. By definition
                   // its scalar value is a clip value
};

struct Point
{
  vtkIdType pid;
  double scalar;
  PointType type;
};

#ifndef NDEBUG
// utility function for debugging: output a polygon point
inline std::ostream& operator<<(std::ostream& os, const Point& p)
{
  constexpr const char* const txt[] = { "V", "CV", "CE" };
  os << "[" << txt[(int)p.type] << ":"
     << "(" << p.pid << ")" << p.scalar << "]";
  return os;
}

// utility function for debugging: output a vector
template <typename T>
std::ostream& operator<<(std::ostream& os, std::vector<T> const& container)
{
  auto b = std::begin(container);
  auto e = std::end(container);

  os << "{";
  auto it = b;
  if (it != e)
    os << *it++;
  for (; it != e; ++it)
    os << "," << *it;
  os << "}";
  return os;
}
#endif

} // unnamed namespace

struct vtkBandedPolyDataContourFilterInternals
{
  // sorted and cleaned contour values
  std::vector<double> ClipValues;
  int ClipIndex[2];     // indices outside of this range (inclusive) are clipped
  double ClipTolerance; // used to clean up numerical problems

  // Find the clip value for val, i.e. the largest clip value <= val+tol
  // Expects that the scalar range minimum and maximum are included in the
  // range [b,e)
  template <typename It>
  It ComputeClipValue(double val, It b, It e)
  {
    It iter = std::upper_bound(b, e, val + ClipTolerance / 2);
    if (b != iter)
      --iter;
    assert(*iter <= val + ClipTolerance / 2 || iter == b);
    return iter;
  }

  std::vector<double>::iterator ComputeClipValue(double val)
  {
    return ComputeClipValue(val, ClipValues.begin(), ClipValues.end());
  }

  template <typename It>
  bool IsClipValue(double val, It clip)
  {
    assert(clip != ClipValues.end());
    return *clip >= val - ClipTolerance / 2 && *clip <= val + ClipTolerance / 2;
  }

  double ComputeClipScalar(double val)
  {
    auto iter = ComputeClipValue(val);
    assert(iter != ClipValues.end());
    return *iter;
  }

  int ComputeClipIndex(double val)
  {
    return static_cast<int>(std::distance(ClipValues.begin(), ComputeClipValue(val)));
  }
};

//------------------------------------------------------------------------------
// Construct object.
vtkBandedPolyDataContourFilter::vtkBandedPolyDataContourFilter()
{
  Internal = new vtkBandedPolyDataContourFilterInternals;

  this->ContourValues = vtkContourValues::New();
  this->Clipping = 0;
  this->ScalarMode = VTK_SCALAR_MODE_INDEX;
  this->Component = 0;

  this->SetNumberOfOutputPorts(2);

  vtkPolyData* output2 = vtkPolyData::New();
  this->GetExecutive()->SetOutputData(1, output2);
  output2->Delete();
  this->ClipTolerance = FLT_EPSILON;
  this->Internal->ClipTolerance = FLT_EPSILON;
  this->GenerateContourEdges = 0;
}

//------------------------------------------------------------------------------
vtkBandedPolyDataContourFilter::~vtkBandedPolyDataContourFilter()
{
  this->ContourValues->Delete();
  delete this->Internal;
}

//------------------------------------------------------------------------------
// Interpolate the input scalars and create intermediate points between
// v1 and v2 at the contour values.
// The point ids are returned in the edgePts array, arranged from v1 to v2 if
// v1<v2 or vice-versa.
// The input array edgePts must be large enough to hold the point ids.
// Return the number of intersection points created in edgePts.
int vtkBandedPolyDataContourFilter::ClipEdge(int v1, int v2, vtkPoints* newPts,
  vtkDataArray* inScalars, vtkDoubleArray* outScalars, vtkPointData* inPD, vtkPointData* outPD,
  vtkIdType edgePts[])
{
  double low = inScalars->GetComponent(v1, this->Component);
  double high = inScalars->GetComponent(v2, this->Component);
  auto b = this->Internal->ComputeClipValue(low);
  auto e = this->Internal->ComputeClipValue(high);
  assert(e != this->Internal->ClipValues.end());

  if (b == e)
  {
    return 0;
  }

  double x[3], x1[3], x2[3];
  newPts->GetPoint(v1, x1);
  newPts->GetPoint(v2, x2);

  bool reverse = (v1 > v2);
  if (low > high)
  {
    std::swap(low, high);
    std::swap(b, e);
    std::swap(x1[0], x2[0]);
    std::swap(x1[1], x2[1]);
    std::swap(x1[2], x2[2]);
    reverse = !reverse;
  }

  // start with the first clip value larger than low
  ++b;
  // include the clipvalue for high
  ++e;

  vtkIdType* pt = reverse ? edgePts + std::distance(b, e) - 1 : edgePts;
  const int inc = reverse ? -1 : +1;
  for (auto iter = b; iter != e; ++iter)
  {
    double t = (*iter - low) / (high - low);
    x[0] = x1[0] + t * (x2[0] - x1[0]);
    x[1] = x1[1] + t * (x2[1] - x1[1]);
    x[2] = x1[2] + t * (x2[2] - x1[2]);
    vtkIdType ptId = newPts->InsertNextPoint(x);
    outPD->InterpolateEdge(inPD, ptId, v1, v2, t);
    outScalars->InsertTypedComponent(ptId, 0, *iter);
    *pt = ptId;
    pt += inc;
  }
  return std::distance(b, e);
}

//------------------------------------------------------------------------------
inline int vtkBandedPolyDataContourFilter::InsertCell(
  vtkCellArray* cells, int npts, const vtkIdType* pts, int cellId, double s, vtkFloatArray* newS)
{
  int idx = this->ComputeClippedIndex(s);
  if (idx < 0)
  {
    return cellId;
  }
  cells->InsertNextCell(npts, pts);
  return InsertNextScalar(newS, cellId, idx);
}

//------------------------------------------------------------------------------
inline int vtkBandedPolyDataContourFilter::InsertLine(
  vtkCellArray* cells, vtkIdType pt1, vtkIdType pt2, int cellId, double s, vtkFloatArray* newS)
{
  int idx = this->ComputeClippedIndex(s);
  if (idx < 0)
  {
    return cellId;
  }
  cells->InsertNextCell(2);
  cells->InsertCellPoint(pt1);
  cells->InsertCellPoint(pt2);
  return InsertNextScalar(newS, cellId, idx);
}

//------------------------------------------------------------------------------
int vtkBandedPolyDataContourFilter::ComputeClippedIndex(double s)
{
  int idx = this->Internal->ComputeClipIndex(s);

  if (!this->Clipping ||
    (idx >= this->Internal->ClipIndex[0] && idx < this->Internal->ClipIndex[1]))
  {
    return idx;
  }
  return -1;
}

//------------------------------------------------------------------------------
int vtkBandedPolyDataContourFilter::InsertNextScalar(vtkFloatArray* scalars, int cellId, int idx)
{
  if (idx < 0)
  {
    return cellId;
  }

  if (this->ScalarMode == VTK_SCALAR_MODE_INDEX)
  {
    double value = idx;
    scalars->InsertTypedComponent(cellId++, 0, value);
  }
  else
  {
    scalars->InsertTypedComponent(cellId++, 0, this->Internal->ClipValues[idx]);
  }
  return cellId;
}

//------------------------------------------------------------------------------
// Create filled contours for polydata
int vtkBandedPolyDataContourFilter::RequestData(vtkInformation* vtkNotUsed(request),
  vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  // get the info objects
  vtkInformation* inInfo = inputVector[0]->GetInformationObject(0);
  vtkInformation* outInfo = outputVector->GetInformationObject(0);

  // get the input and output
  vtkPolyData* input = vtkPolyData::SafeDownCast(inInfo->Get(vtkDataObject::DATA_OBJECT()));
  vtkPolyData* output = vtkPolyData::SafeDownCast(outInfo->Get(vtkDataObject::DATA_OBJECT()));

  vtkPointData* pd = input->GetPointData();
  vtkPointData* outPD = output->GetPointData();
  vtkCellData* outCD = output->GetCellData();
  vtkPoints* inPts = input->GetPoints();
  vtkDataArray* inScalars = pd->GetScalars();
  int abort = 0;
  vtkPoints* newPts;
  vtkIdType npts = 0;
  vtkIdType cellId = 0;
  const vtkIdType* pts = nullptr;
  int numEdgePts, maxCellSize;
  vtkIdType v;
  vtkIdType vR;
  const vtkIdType* intPts;
  vtkIdType intCellId;
  vtkIdType numIntPts;
  vtkIdType numPts, numCells, estimatedSize;

  vtkDebugMacro(<< "Executing banded contour filter");

  //  Check input
  //

  numCells = input->GetNumberOfCells();
  if (!inPts || (numPts = inPts->GetNumberOfPoints()) < 1 || !inScalars || numCells < 1)
  {
    vtkErrorMacro(<< "No input data!");
    return 1;
  }

  if (inScalars->GetNumberOfComponents() < this->Component + 1)
  {
    vtkErrorMacro(<< "Input scalars expected to have " << this->Component + 1 << " components");
    return 0;
  }

  if (this->ContourValues->GetNumberOfContours() < 1)
  {
    vtkWarningMacro(<< "No contour values");
    return 1;
  }

  // Set up supplemental data structures for processing edge/generating
  // intersections. First we sort the contour values into an ascending
  // list of clip values including the extreme min/max values.
  double range[2];
  inScalars->GetRange(range);

  // base clip tolerance on overall input scalar range
  this->Internal->ClipTolerance = this->ClipTolerance * (range[1] - range[0]);

  // sort the contour values
  std::vector<double> contourValues;
  contourValues.reserve(this->ContourValues->GetNumberOfContours());
  for (int i = 0; i < this->ContourValues->GetNumberOfContours(); ++i)
  {
    contourValues.push_back(this->ContourValues->GetValue(i));
  }
  std::sort(contourValues.begin(), contourValues.end());

  // Copy the sorted contour values, and the range minimum and maximum if they
  // exceed the contour value extremes
  this->Internal->ClipValues.clear();
  if (range[0] < contourValues.front())
  {
    this->Internal->ClipValues.push_back(range[0]);
  }
  std::copy(
    contourValues.begin(), contourValues.end(), std::back_inserter(this->Internal->ClipValues));
  if (range[1] > contourValues.back())
  {
    this->Internal->ClipValues.push_back(range[1]);
  }

  // Remove clipvalues that are less than the internal tolerance of each other
  auto iter1 = this->Internal->ClipValues.begin();
  for (auto iter2 = iter1 + 1; iter2 != this->Internal->ClipValues.end(); ++iter2)
  {
    iter2 = std::upper_bound(
      iter2, this->Internal->ClipValues.end(), *iter1 + this->Internal->ClipTolerance);
    if (iter2 != this->Internal->ClipValues.end())
    {
      *(++iter1) = *iter2;
    }
    else
    {
      break;
    }
  }
  this->Internal->ClipValues.resize(std::distance(this->Internal->ClipValues.begin(), iter1 + 1));
  const int numClipValues = static_cast<int>(this->Internal->ClipValues.size());

  this->Internal->ClipIndex[0] = this->Internal->ComputeClipIndex(this->ContourValues->GetValue(0));
  this->Internal->ClipIndex[1] = this->Internal->ComputeClipIndex(
    this->ContourValues->GetValue(this->ContourValues->GetNumberOfContours() - 1));

  //
  // Estimate allocation size, stolen from vtkContourGrid...
  //
  estimatedSize = static_cast<vtkIdType>(pow(static_cast<double>(numCells), .9));
  estimatedSize *= numClipValues;
  estimatedSize = estimatedSize / 1024 * 1024; // multiple of 1024
  if (estimatedSize < 1024)
  {
    estimatedSize = 1024;
  }

  // The original set of points and point data are copied. Later on
  // intersection points due to clipping will be created.
  newPts = vtkPoints::New();

  // Note: since we use the output scalars in the execution of the algorithm,
  // the output point scalars MUST BE double or bad things happen due to
  // numerical precision issues.
  newPts->Allocate(estimatedSize, estimatedSize);
  outPD->CopyScalarsOff();
  outPD->InterpolateAllocate(pd, 3 * numPts, numPts);
  vtkDoubleArray* outScalars = vtkDoubleArray::New();
  outScalars->Allocate(3 * numPts, numPts);
  outPD->SetScalars(outScalars);
  outScalars->Delete();

  for (int i = 0; i < numPts; i++)
  {
    newPts->InsertPoint(i, inPts->GetPoint(i));
    outPD->CopyData(pd, i, i);
    double value = inScalars->GetComponent(i, this->Component);
    outScalars->InsertTypedComponent(i, 0, value);
  }

  // These are the new cell scalars
  vtkFloatArray* newScalars = vtkFloatArray::New();
  newScalars->Allocate(numCells * 5, numCells);
  newScalars->SetName("Scalars");

  // Used to keep track of intersections
  vtkEdgeTable* edgeTable = vtkEdgeTable::New();
  vtkCellArray* intList = vtkCellArray::New(); // intersection point ids

  // All vertices are filled and passed through; poly-vertices are broken
  // into single vertices. Cell data per vertex is set.
  //
  if (input->GetVerts()->GetNumberOfCells() > 0)
  {
    vtkCellArray* verts = input->GetVerts();
    vtkCellArray* newVerts = vtkCellArray::New();
    newVerts->AllocateCopy(verts);
    for (verts->InitTraversal(); verts->GetNextCell(npts, pts) && !abort;
         abort = this->GetAbortExecute())
    {
      for (int i = 0; i < npts; i++)
      {
        cellId = this->InsertCell(newVerts, 1, pts + i, cellId,
          inScalars->GetComponent(pts[i], this->Component), newScalars);
      }
    }
    output->SetVerts(newVerts);
    newVerts->Delete();
  }
  this->UpdateProgress(0.05);

  // Lines are chopped into line segments.
  //
  if (input->GetLines()->GetNumberOfCells() > 0)
  {
    vtkCellArray* lines = input->GetLines();

    maxCellSize = lines->GetMaxCellSize();
    maxCellSize *= (1 + numClipValues);

    vtkIdType* fullLine = new vtkIdType[maxCellSize];
    vtkCellArray* newLines = vtkCellArray::New();
    newLines->AllocateCopy(lines);
    edgeTable->InitEdgeInsertion(numPts, 1); // store attributes on edge

    // start by generating intersection points
    for (lines->InitTraversal(); lines->GetNextCell(npts, pts) && !abort;
         abort = this->GetAbortExecute())
    {
      for (int i = 0; i < (npts - 1); i++)
      {
        numEdgePts =
          this->ClipEdge(pts[i], pts[i + 1], newPts, inScalars, outScalars, pd, outPD, fullLine);
        if (numEdgePts > 0) // there is an intersection
        {
          intList->InsertNextCell(numEdgePts, fullLine);
          edgeTable->InsertEdge(pts[i], pts[i + 1], // associate ints with edge
            intList->GetNumberOfCells() - 1);
        }
        else // no intersection points along the edge
        {
          edgeTable->InsertEdge(pts[i], pts[i + 1], -1); //-1 means no points
        }
      } // for all line segments in this line
    }

    // now create line segments
    for (lines->InitTraversal(); lines->GetNextCell(npts, pts) && !abort;
         abort = this->GetAbortExecute())
    {
      for (int i = 0; i < (npts - 1); i++)
      {
        v = pts[i];
        vR = pts[i + 1];
        bool reverse = (v > vR);

        double s1 = inScalars->GetComponent(v, this->Component);
        double s2 = inScalars->GetComponent(vR, this->Component);
        bool increasing = (s2 > s1);

        vtkIdType p1 = v;
        if ((intCellId = edgeTable->IsEdge(v, vR)) != -1)
        {
          intList->GetCellAtId(intCellId, numIntPts, intPts);
          int incr;
          int k;
          if (!reverse)
          {
            k = 0;
            incr = 1;
          }
          else
          {
            k = numIntPts - 1;
            incr = -1;
          }
          for (int n = 0; n < numIntPts; ++n, k += incr)
          {
            vtkIdType p2 = intPts[k];
            double value = outScalars->GetTypedComponent(increasing ? p1 : p2, 0);
            cellId = this->InsertLine(newLines, p1, p2, cellId, value, newScalars);
            p1 = p2;
          }
          double value = outScalars->GetTypedComponent(increasing ? p1 : vR, 0);
          cellId = this->InsertLine(newLines, p1, vR, cellId, value, newScalars);
        }
        else
        {
          double value = outScalars->GetTypedComponent(vR, 0);
          cellId = this->InsertLine(newLines, v, vR, cellId, value, newScalars);
        }
      }
    }

    delete[] fullLine;

    output->SetLines(newLines);
    newLines->Delete();
  }
  this->UpdateProgress(0.1);

  // Polygons are assumed convex and chopped into filled, convex polygons.
  // Triangle strips are treated similarly.
  //
  vtkIdType numPolys = input->GetPolys()->GetNumberOfCells();
  vtkIdType numStrips = input->GetStrips()->GetNumberOfCells();
  if (numPolys > 0 || numStrips > 0)
  {
    // Set up processing. We are going to store an ordered list of
    // intersections along each edge (ordered from smallest point id
    // to largest). These will later be connected into convex polygons
    // which represent a filled region in the cell.
    //
    edgeTable->InitEdgeInsertion(numPts, 1); // store attributes on edge
    intList->Reset();

    vtkCellArray* polys = input->GetPolys();
    vtkCellArray* tmpPolys = nullptr;

    // If contour edges requested, set things up.
    vtkCellArray* contourEdges = nullptr;
    if (this->GenerateContourEdges)
    {
      contourEdges = vtkCellArray::New();
      contourEdges->AllocateEstimate(numCells, 2);
      this->GetContourEdgesOutput()->SetLines(contourEdges);
      contourEdges->Delete();
      this->GetContourEdgesOutput()->SetPoints(newPts);
    }

    // Set up structures for processing polygons
    maxCellSize = polys->GetMaxCellSize();
    if (maxCellSize == 0)
    {
      maxCellSize = input->GetStrips()->GetMaxCellSize();
    }
    maxCellSize *= (1 + numClipValues);

    std::vector<vtkIdType> pointIds;
    pointIds.reserve(maxCellSize);

    // Lump strips and polygons together.
    // Decompose strips into triangles.
    if (numStrips > 0)
    {
      vtkCellArray* strips = input->GetStrips();
      tmpPolys = vtkCellArray::New();
      if (numPolys > 0)
      {
        tmpPolys->DeepCopy(polys);
      }
      else
      {
        tmpPolys->AllocateEstimate(numStrips, 5);
      }
      for (strips->InitTraversal(); strips->GetNextCell(npts, pts);)
      {
        vtkTriangleStrip::DecomposeStrip(npts, pts, tmpPolys);
      }
      polys = tmpPolys;
    }

    // Process polygons to produce edge intersections.------------------------
    //
    numPolys = polys->GetNumberOfCells();
    vtkIdType updateCount = numPolys / 20 + 1;
    vtkIdType count = 0;
    pointIds.resize(this->Internal->ClipValues.size(), -1);
    for (polys->InitTraversal(); polys->GetNextCell(npts, pts) && !abort;
         abort = this->GetAbortExecute())
    {
      if (!(++count % updateCount))
      {
        this->UpdateProgress(0.1 + 0.45 * (static_cast<double>(count) / numPolys));
      }

      for (int i = 0; i < npts; i++)
      {
        v = pts[i];
        vR = pts[(i + 1) % npts];
        if (edgeTable->IsEdge(v, vR) == -1)
        {
          numEdgePts =
            this->ClipEdge(v, vR, newPts, inScalars, outScalars, pd, outPD, &pointIds[0]);
          if (numEdgePts > 0)
          {
            intList->InsertNextCell(numEdgePts, &pointIds[0]);
            edgeTable->InsertEdge(v, vR, // associate ints with edge
              intList->GetNumberOfCells() - 1);
          }
          else // no intersection points along the edge
          {
            edgeTable->InsertEdge(v, vR, NO_INTERSECTION);
          }
        } // if edge not processed yet
      }
    } // for all polygons

    // Process polygons to produce output triangles------------------------
    //
    vtkCellArray* newPolys = vtkCellArray::New();
    newPolys->AllocateCopy(polys);
    count = 0;

    // polygon point ids, point types, scalars
    std::vector<Point> polygon;
    polygon.reserve(maxCellSize + 1);

    // indices into the polygon point vector
    std::vector<int> index;
    index.reserve(maxCellSize + 1);

    for (polys->InitTraversal(); polys->GetNextCell(npts, pts) && !abort;
         abort = this->GetAbortExecute())
    {
      if (!(++count % updateCount))
      {
        this->UpdateProgress(0.55 + 0.45 * (static_cast<double>(count) / numPolys));
      }

      // Create a new polygon that includes all the points including the
      // intersection vertices. This hugely simplifies the logic of the
      // code.
      polygon.clear();
      index.clear();
      bool hasClippedEdges = false;
      for (int i = 0; i < npts; i++)
      {
        v = pts[i];
        vR = pts[(i + 1) % npts];

        double scalar = outScalars->GetTypedComponent(v, 0);
        auto iter = this->Internal->ComputeClipValue(scalar);
        const bool isClip = this->Internal->IsClipValue(scalar, iter);
        polygon.push_back(
          { v, isClip ? *iter : scalar, (isClip ? PointType::CLIP_VERTEX : PointType::VERTEX) });

        // see whether intersection points need to be added.
        intCellId = edgeTable->IsEdge(v, vR);
        if (intCellId != -1 && intCellId != NO_INTERSECTION)
        {
          hasClippedEdges = true;
          intList->GetCellAtId(intCellId, numIntPts, intPts);
          int first, last, inc;
          if (v < vR)
          {
            first = 0;
            last = numIntPts;
            inc = 1;
          }
          else
          {
            first = numIntPts - 1;
            last = -1;
            inc = -1;
          }
          for (int k = first; k != last; k += inc)
          {
            polygon.push_back(
              { intPts[k], outScalars->GetTypedComponent(intPts[k], 0), PointType::EDGE });
          }
        }
      } // for all points and edges

      auto point_less = [](const Point& p1, const Point& p2) { return p1.scalar < p2.scalar; };

      // Trivial output - completely in a contour band or a triangle
      if (!hasClippedEdges || polygon.size() == 3)
      {
        auto it = std::min_element(polygon.begin(), polygon.end(), point_less);
        cellId = this->InsertCell(newPolys, npts, pts, cellId, it->scalar, newScalars);
        continue;
      }

      // Initialize the indexing array. Starts with the starting vertex, and
      // then iterates around the polygon.
      index.resize(polygon.size());
      std::iota(index.begin(), index.end(), 0);

      // Find the starting vertex, i.e. the vertex with the lowest scalar value,
      // and rotate the indexing array such that it is the first of the indices
      auto indexed_less = [&polygon, &point_less](
                            int i1, int i2) { return point_less(polygon[i1], polygon[i2]); };
      std::rotate(
        index.begin(), std::min_element(index.begin(), index.end(), indexed_less), index.end());

      // Add a duplicate of the starting vertex at the end to avoid having to
      // test for validity of iterators before dereferencing. Note that the
      // duplicate of the point is never referenced from the indexing array.
      index.push_back(index.front());     // add another idx at the end
      polygon.push_back(polygon.front()); // and a copy of the point

      // Contour edges at the boundaries of the cell
      if (this->GenerateContourEdges)
      {
        for (auto it = index.begin(); it != index.end() - 1; ++it)
        {
          const auto& p1 = polygon[*it];
          const auto& p2 = polygon[*(it + 1)];
          if (p1.type != PointType::VERTEX && p2.type != PointType::VERTEX &&
            p1.scalar == p2.scalar)
          {
            contourEdges->InsertNextCell(2);
            contourEdges->InsertCellPoint(p1.pid);
            contourEdges->InsertCellPoint(p2.pid);
          }
        }
      }

      // start from the lowest clipvalue
      double clip_scalar = this->Internal->ComputeClipScalar(polygon[index.front()].scalar);

      vtkDebugMacro(<< "clip_scalar=" << clip_scalar << "\n"
                    << "\tpolygon=" << polygon << "\n"
                    << "\tindex=" << index);

      // traverse the polygon points from the starting vertex going
      // left/clockwise (reverse through indices) and
      // right/counter-clockwise (forward through indices)
      typedef std::vector<int>::iterator It;
      typedef std::reverse_iterator<It> RevIt;
      It r1 = index.begin();
      It l1 = index.end() - 1;
      while (r1 < l1)
      {
        auto in_band = [&clip_scalar, &polygon](int i) {
          return (polygon[i].scalar == clip_scalar) ||
            ((polygon[i].type == PointType::VERTEX && polygon[i].scalar > clip_scalar));
        };

        assert(polygon[*l1].type == PointType::VERTEX || polygon[*r1].type == PointType::VERTEX ||
          polygon[*l1].scalar == polygon[*r1].scalar);
        assert(in_band(*r1));
        assert(in_band(*l1));

        // find next left and right band ends
        auto r2 = std::find_if_not(r1, l1, in_band);
        auto l2 = std::find_if_not(RevIt(l1), RevIt(r2), in_band).base() - 1;

        vtkDebugMacro(<< "band: clip_scalar=" << clip_scalar << " points=[" << *l2 << polygon[*l2]
                      << " -> " << *l1 << polygon[*l1] << " -> " << *r1 << polygon[*r1] << " -> "
                      << *r2 << polygon[*r2] << "]");

        // If r2 or l2 refers to a point with a scalar smaller than the
        // current clip scalar, it is on an edge with decreasing scalars.
        //
        // Restart contouring of the remaining polygon by discarding points
        // of lower clip values (i.e. points already traversed by r1 and l1),
        // find the new vertex with lowest scalar and initialize iterators and
        // clip_scalar
        if ((polygon[*l2].scalar < clip_scalar) || (polygon[*r2].scalar < clip_scalar))
        {
          auto it = index.begin() + std::distance(r1, l1 + 1);
          std::rotate(index.begin(), r1, l1 + 1);
          // note: the duplicate at the end is automatically discarded
          index.resize(std::distance(index.begin(), it));

          // find the index of the new starting vertex
          auto indexed_vertex_scalar_less = [&polygon](int i1, int i2) {
            return (
              (polygon[i1].type != PointType::EDGE) && (polygon[i1].scalar < polygon[i2].scalar));
          };
          it = std::min_element(index.begin(), index.end(), indexed_vertex_scalar_less);
          std::rotate(index.begin(), it, index.end());
          index.push_back(index.front()); // duplicate of the first point

          clip_scalar = this->Internal->ComputeClipScalar(polygon[index.front()].scalar);

          vtkDebugMacro(<< "clip_scalar=" << clip_scalar << "\n"
                        << "\tpolygon=" << polygon << "\n"
                        << "\tindex=" << index);

          r1 = index.begin();
          l1 = index.end() - 1;
          continue;
        }

        assert(*l1 == *r2 || // first band
          r2 == l1 ||        // last band
          ((polygon[*l2].type != PointType::VERTEX) && (polygon[*r2].type != PointType::VERTEX) &&
            (polygon[*l2].scalar == polygon[*r2].scalar)));

        // copy point ids from l2 to l1 and from r1 to r2
        auto l = l1 + 1;
        auto r = r2 + 1;
        // do not duplicate the first point
        if (*l1 == *r1)
          --l;
        // for last contour band r1->r2 spans entire polygon
        if (r2 == l1)
          l = l2;
        pointIds.resize(std::distance(l2, l) + std::distance(r1, r));
        if (pointIds.size() >= 3)
        {
          auto copyPointIds = [&polygon](int i) { return polygon[i].pid; };
          auto it = std::transform(l2, l, pointIds.begin(), copyPointIds);
          std::transform(r1, r, it, copyPointIds);
          vtkDebugMacro(<< "clip_scalar=" << clip_scalar << "\n"
                        << " pointIds=" << pointIds);
          cellId = this->InsertCell(newPolys, static_cast<int>(pointIds.size()), &pointIds[0],
            cellId, clip_scalar, newScalars);
          if (this->GenerateContourEdges && r2 != l1)
          {
            contourEdges->InsertNextCell(2);
            contourEdges->InsertCellPoint(polygon[*r2].pid);
            contourEdges->InsertCellPoint(polygon[*l2].pid);
          }
        }
        r1 = r2;
        l1 = l2;
        clip_scalar = polygon[*r1].scalar;
      }
    } // for all polygons

    output->SetPolys(newPolys);
    newPolys->Delete();
    if (tmpPolys)
    {
      tmpPolys->Delete();
    }
  } // for all polygons (and strips) in input

  vtkDebugMacro(<< "Created " << cellId << " total cells\n");
  vtkDebugMacro(<< "Created " << output->GetVerts()->GetNumberOfCells() << " verts\n");
  vtkDebugMacro(<< "Created " << output->GetLines()->GetNumberOfCells() << " lines\n");
  vtkDebugMacro(<< "Created " << output->GetPolys()->GetNumberOfCells() << " polys\n");
  vtkDebugMacro(<< "Created " << output->GetStrips()->GetNumberOfCells() << " strips\n");

  //  Update ourselves and release temporary memory
  //
  intList->Delete();
  edgeTable->Delete();

  output->SetPoints(newPts);
  newPts->Delete();

  int arrayIdx = outCD->AddArray(newScalars);
  outCD->SetActiveAttribute(arrayIdx, vtkDataSetAttributes::SCALARS);

  newScalars->Delete();

  output->Squeeze();

  return 1;
}

//------------------------------------------------------------------------------
vtkPolyData* vtkBandedPolyDataContourFilter::GetContourEdgesOutput()
{
  if (this->GetNumberOfOutputPorts() < 2)
  {
    return nullptr;
  }

  return vtkPolyData::SafeDownCast(this->GetExecutive()->GetOutputData(1));
}

//------------------------------------------------------------------------------
vtkMTimeType vtkBandedPolyDataContourFilter::GetMTime()
{
  vtkMTimeType mTime = this->Superclass::GetMTime();
  vtkMTimeType time;

  time = this->ContourValues->GetMTime();
  mTime = (time > mTime ? time : mTime);

  return mTime;
}

//------------------------------------------------------------------------------
void vtkBandedPolyDataContourFilter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  os << indent << "Generate Contour Edges: " << (this->GenerateContourEdges ? "On\n" : "Off\n");

  this->ContourValues->PrintSelf(os, indent.GetNextIndent());
  os << indent << "Clipping: " << (this->Clipping ? "On\n" : "Off\n");

  os << indent << "Scalar Mode: ";
  if (this->ScalarMode == VTK_SCALAR_MODE_INDEX)
  {
    os << "INDEX\n";
  }
  else
  {
    os << "VALUE\n";
  }

  os << indent << "Clip Tolerance: " << this->ClipTolerance << "\n";
}
