//============================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//============================================================================
// Copyright (c) 2018, The Regents of the University of California, through
// Lawrence Berkeley National Laboratory (subject to receipt of any required approvals
// from the U.S. Dept. of Energy).  All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
// (1) Redistributions of source code must retain the above copyright notice, this
//     list of conditions and the following disclaimer.
//
// (2) Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
// (3) Neither the name of the University of California, Lawrence Berkeley National
//     Laboratory, U.S. Dept. of Energy nor the names of its contributors may be
//     used to endorse or promote products derived from this software without
//     specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
//
//=============================================================================
//
//  This code is an extension of the algorithm presented in the paper:
//  Parallel Peak Pruning for Scalable SMP Contour Tree Computation.
//  Hamish Carr, Gunther Weber, Christopher Sewell, and James Ahrens.
//  Proceedings of the IEEE Symposium on Large Data Analysis and Visualization
//  (LDAV), October 2016, Baltimore, Maryland.
//
//  The PPP2 algorithm and software were jointly developed by
//  Hamish Carr (University of Leeds), Gunther H. Weber (LBNL), and
//  Oliver Ruebel (LBNL)
//==============================================================================

#ifndef vtk_m_worklet_contourtree_distributed_bract_maker_identify_regularise_supernodes_step_two_worklet_h
#define vtk_m_worklet_contourtree_distributed_bract_maker_identify_regularise_supernodes_step_two_worklet_h

#include <vtkm/worklet/WorkletMapField.h>
#include <vtkm/worklet/contourtree_augmented/Types.h>

namespace vtkm
{
namespace worklet
{
namespace contourtree_distributed
{
namespace bract_maker
{

/// Step 2 of IdentifyRegularisedSupernodes
/// We also want to flag the leaves and boundary nodes as necessary
class IdentifyRegularisedSupernodesStepTwoWorklet : public vtkm::worklet::WorkletMapField
{
public:
  using ControlSignature = void(FieldIn bractVertexSuperset, // input
                                FieldIn upNeighbour,         // input
                                FieldIn downNeighbour,       // input
                                ExecObject meshBoundary,     // input
                                WholeArrayOut newVertexId    // output
  );
  using ExecutionSignature = void(InputIndex, _1, _2, _3, _4, _5);
  using InputDomain = _1;

  // Default Constructor
  VTKM_EXEC_CONT
  IdentifyRegularisedSupernodesStepTwoWorklet() {}

  template <typename MeshBoundaryType, typename OutFieldPortalType>
  VTKM_EXEC void operator()(const vtkm::Id& boundaryNode,
                            const vtkm::Id& bractVertexSupersetValue,
                            const vtkm::Id& upNeighbourValue,
                            const vtkm::Id& downNeighbourValue,
                            const MeshBoundaryType& meshBoundary,
                            const OutFieldPortalType& newVertexIdPortal) const
  {
    // per vertex
    // check for leaf criticality
    if (vtkm::worklet::contourtree_augmented::NoSuchElement(upNeighbourValue) ||
        vtkm::worklet::contourtree_augmented::NoSuchElement(downNeighbourValue))
    {
      newVertexIdPortal.Set(boundaryNode,
                            (vtkm::Id)vtkm::worklet::contourtree_augmented::ELEMENT_EXISTS);
    }
    // retrieve mesh index (i.e., bractVertexSupersetValue) & check whether on the boundary
    if (
      meshBoundary.LiesOnBoundary(
        bractVertexSupersetValue)) // TODO: This may need to change to isNecessary when we change to boundary critical points
    {
      newVertexIdPortal.Set(boundaryNode,
                            (vtkm::Id)vtkm::worklet::contourtree_augmented::ELEMENT_EXISTS);
    }

    // In serial this worklet implements the following operation
    /*
    for (indexType boundaryNode = 0; boundaryNode < bractVertexSuperset.size(); boundaryNode++)
    { // per vertex
      // check for leaf criticality
      if (noSuchElement(upNeighbour[boundaryNode]) || noSuchElement(downNeighbour[boundaryNode]))
        newVertexID[boundaryNode] = ELEMENT_EXISTS;
      // retrieve mesh index & check whether on the boundary
      indexType meshIndex = bractVertexSuperset[boundaryNode];
      if (mesh->liesOnBoundary(meshIndex))
        newVertexID[boundaryNode] = ELEMENT_EXISTS;
    } // per vertex
    */
  }
}; // IdentifyRegularisedSupernodesStepTwoWorklet


} // namespace bract_maker
} // namespace contourtree_distributed
} // namespace worklet
} // namespace vtkm

#endif