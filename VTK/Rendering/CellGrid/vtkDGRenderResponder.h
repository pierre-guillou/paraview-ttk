// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @class   vtkDGRenderResponder
 * @brief   Rendering simple DG cells (i.e., those with a fixed reference shape).
 *
 * This currently handles hexahedra, tetrahedra, quadrilaterals, and triangles.
 */

#ifndef vtkDGRenderResponder_h
#define vtkDGRenderResponder_h

#include "vtkCellGridRenderRequest.h"

#include "vtkCellGridResponder.h"       // For API.
#include "vtkDGCell.h"                  // For API.
#include "vtkDrawTexturedElements.h"    // For CacheEntry.
#include "vtkInformation.h"             // For CacheEntry.
#include "vtkRenderingCellGridModule.h" // For Export macro

#include <string>
#include <unordered_set>
#include <vector>

#include "vtk_glew.h" // For API.

VTK_ABI_NAMESPACE_BEGIN

class vtkCellMetadata;
class vtkDGCell;
class vtkDGRenderResponders;
class vtkCellAttributeInformation;

class VTKRENDERINGCELLGRID_EXPORT vtkDGRenderResponder
  : public vtkCellGridResponder<vtkCellGridRenderRequest>
{
public:
  static vtkDGRenderResponder* New();
  vtkTypeMacro(vtkDGRenderResponder, vtkCellGridResponder<vtkCellGridRenderRequest>);

  bool Query(vtkCellGridRenderRequest* request, vtkCellMetadata* metadata,
    vtkCellGridResponders* caches) override;

  void AddMod(const std::string& className);
  void AddMods(const std::vector<std::string>& classNames);
  void RemoveMod(const std::string& className);
  void RemoveAllMods();

  /// If you removed all mods, call this to go back to default setting.
  void ResetModsToDefault();

protected:
  vtkDGRenderResponder();
  ~vtkDGRenderResponder() override = default;

  bool DrawCells(vtkCellGridRenderRequest* request, vtkCellMetadata* metadata);
  bool ReleaseResources(vtkCellGridRenderRequest* request, vtkCellMetadata* metadata);

  bool DrawShapes(
    vtkCellGridRenderRequest* request, vtkDGCell* metadata, const vtkDGCell::Source& cellSource);
  // std::pair<GLenum, std::vector<GLubyte>> ShapeToPrimitiveInfo(int shapeId);

  /// Entries for a cache of render-helpers.
  struct CacheEntry
  {
    /// @name Cache keys
    /// These variables are used by the comparator as a key.
    ///@{
    /// The cell-type within the grid to be rendered.
    vtkSmartPointer<vtkDGCell> CellType;
    /// The cell- or side-source within the cell-type to be rendered.
    const vtkDGCell::Source* CellSource;
    /// The vector-valued attribute used to move from reference coordinates to world coordinates.
    vtkSmartPointer<vtkCellAttribute> Shape;
    /// The attribute used to color the geometry (or null).
    vtkSmartPointer<vtkCellAttribute> Color;
    /// The last render pass information is cached when \a RenderHelper is configured.
    vtkNew<vtkInformation> LastRenderPassInfo;
    ///@}

    /// @name Cache data
    /// These variables may be modified during a render.
    /// They are not used to sort/index the entry, so are mutable.
    ///@{
    /// A container for arrays and shaders that actually draws the data.
    mutable std::unique_ptr<vtkDrawTexturedElements> RenderHelper;
    /// The MTime of the shape cell-attribute at the time \a RenderHelper was configured.
    mutable vtkMTimeType ShapeTime;
    /// The MTime of the color cell-attribute at the time \a RenderHelper was configured.
    mutable vtkMTimeType ColorTime;
    /// The MTime of the cell-grid which owns \a CellType at the time \a RenderHelper was
    /// configured.
    mutable vtkMTimeType GridTime;
    /// The MTime of the property at the time \a RenderHelper was configured.
    /// The actor controls default visual properties (color, opacity, transforms)
    /// applied to the cell-grid.
    mutable vtkMTimeType PropertyTime;
    /// The MTime of the mapper at the time \a RenderHelper was configured.
    /// The mapper controls what component is used for coloring,
    mutable vtkMTimeType MapperTime;
    /// The MTime of the render passes combined at the time \a RenderHelper was configured.
    /// Various render passes are injected into the render pipeline by vtkOpenGLRenderer for fancy
    /// features like dual-depth peeling, SSAO, etc.
    mutable vtkMTimeType RenderPassStageTime;
    ///@}

    /// @name Mods
    /// These are the names of classes which are subclasess of vtkGLSLRuntimeModBase.
    /// The mods will be loaded one by one and applied in the order they were added.
    std::vector<std::string> ModNames;

    /// Determine whether to remove this cache entry because
    /// \a renderer, \a actor, or \a mapper have changed since
    /// it was constructed.
    bool IsUpToDate(vtkRenderer* renderer, vtkActor* actor, vtkMapper* mapper,
      vtkObject* debugAttachment = nullptr) const;

    /// Allocate a RenderHelper as needed and configure it.
    void PrepareHelper(vtkRenderer* renderer, vtkActor* actor, vtkMapper* mapper) const;

    /// Used sort cache entries for inclusion in a std::set<>.
    bool operator<(const CacheEntry& other) const;
  };

  std::set<CacheEntry> Helpers;
  static vtkDrawTexturedElements::ElementShape PrimitiveFromShape(vtkDGCell::Shape shape);

  std::vector<std::string> ModNames;
  std::set<std::string> ModNamesUnique;
  static std::vector<std::string> DefaultModNames;

private:
  vtkDGRenderResponder(const vtkDGRenderResponder&) = delete;
  void operator=(const vtkDGRenderResponder&) = delete;
};

VTK_ABI_NAMESPACE_END
#endif // vtkDGRenderResponder_h
// VTK-HeaderTest-Exclude: vtkDGRenderResponder.h
