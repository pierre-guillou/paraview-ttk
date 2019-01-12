/*=========================================================================

  Program:   ParaView
  Module:    vtkGeometryRepresentation.h

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkGeometryRepresentation
 * @brief   representation for showing any datasets as
 * external shell of polygons.
 *
 * vtkGeometryRepresentation is a representation for showing polygon geometry.
 * It handles non-polygonal datasets by extracting external surfaces. One can
 * use this representation to show surface/wireframe/points/surface-with-edges.
 * @par Thanks:
 * The addition of a transformation matrix was supported by CEA/DIF
 * Commissariat a l'Energie Atomique, Centre DAM Ile-De-France, Arpajon, France.
*/

#ifndef vtkGeometryRepresentation_h
#define vtkGeometryRepresentation_h
#include <array>         // needed for array
#include <unordered_map> // needed for unordered_map

#include "vtkPVClientServerCoreRenderingModule.h" // needed for exports
#include "vtkPVDataRepresentation.h"
#include "vtkProperty.h" // needed for VTK_POINTS etc.

class vtkCallbackCommand;
class vtkCompositeDataDisplayAttributes;
class vtkCompositePolyDataMapper2;
class vtkMapper;
class vtkPiecewiseFunction;
class vtkPVCacheKeeper;
class vtkPVGeometryFilter;
class vtkPVLODActor;
class vtkScalarsToColors;
class vtkTexture;

namespace vtkGeometryRepresentation_detail
{
// This is defined to either vtkQuadricClustering or vtkmLevelOfDetail in the
// implementation file:
class DecimationFilterType;
}

class VTKPVCLIENTSERVERCORERENDERING_EXPORT vtkGeometryRepresentation
  : public vtkPVDataRepresentation
{

public:
  static vtkGeometryRepresentation* New();
  vtkTypeMacro(vtkGeometryRepresentation, vtkPVDataRepresentation);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  /**
   * vtkAlgorithm::ProcessRequest() equivalent for rendering passes. This is
   * typically called by the vtkView to request meta-data from the
   * representations or ask them to perform certain tasks e.g.
   * PrepareForRendering.
   */
  int ProcessViewRequest(vtkInformationRequestKey* request_type, vtkInformation* inInfo,
    vtkInformation* outInfo) VTK_OVERRIDE;

  /**
   * This needs to be called on all instances of vtkGeometryRepresentation when
   * the input is modified. This is essential since the geometry filter does not
   * have any real-input on the client side which messes with the Update
   * requests.
   */
  void MarkModified() VTK_OVERRIDE;

  /**
   * Get/Set the visibility for this representation. When the visibility of
   * representation of false, all view passes are ignored.
   */
  void SetVisibility(bool val) VTK_OVERRIDE;

  //@{
  /**
   * Determines the number of distinct values in vtkBlockColors
   * See also vtkPVGeometryFilter
   */
  void SetBlockColorsDistinctValues(int distinctValues);
  int GetBlockColorsDistinctValues();
  //@}

  /**
   * Enable/Disable LOD;
   */
  virtual void SetSuppressLOD(bool suppress) { this->SuppressLOD = suppress; }

  //@{
  /**
   * Set the lighting properties of the object. vtkGeometryRepresentation
   * overrides these based of the following conditions:
   * \li When Representation is wireframe or points, it disables diffuse or
   * specular.
   * \li When scalar coloring is employed, it disabled specular.
   */
  vtkSetMacro(Ambient, double);
  vtkSetMacro(Diffuse, double);
  vtkSetMacro(Specular, double);
  vtkGetMacro(Ambient, double);
  vtkGetMacro(Diffuse, double);
  vtkGetMacro(Specular, double);
  //@}

  enum RepresentationTypes
  {
    POINTS = VTK_POINTS,
    WIREFRAME = VTK_WIREFRAME,
    SURFACE = VTK_SURFACE,
    SURFACE_WITH_EDGES = 3
  };

  //@{
  /**
   * Set the representation type. This adds VTK_SURFACE_WITH_EDGES to those
   * defined in vtkProperty.
   */
  vtkSetClampMacro(Representation, int, POINTS, SURFACE_WITH_EDGES);
  vtkGetMacro(Representation, int);
  //@}

  /**
   * Overload to set representation type using string. Accepted strings are:
   * "Points", "Wireframe", "Surface" and "Surface With Edges".
   */
  virtual void SetRepresentation(const char*);

  /**
   * Returns the data object that is rendered from the given input port.
   */
  vtkDataObject* GetRenderedDataObject(int port) VTK_OVERRIDE;

  /**
   * Returns true if this class would like to get ghost-cells if available for
   * the connection whose information object is passed as the argument.
   * @deprecated in ParaView 5.5. See
   * `vtkProcessModule::GetNumberOfGhostLevelsToRequest` instead.
   */
  VTK_LEGACY(static bool DoRequestGhostCells(vtkInformation* information));

  //@{
  /**
   * Representations that use geometry representation as the internal
   * representation should turn this flag off so that we don't end up requesting
   * ghost cells twice.
   */
  vtkSetMacro(RequestGhostCellsIfNeeded, bool);
  vtkGetMacro(RequestGhostCellsIfNeeded, bool);
  vtkBooleanMacro(RequestGhostCellsIfNeeded, bool);
  //@}

  //***************************************************************************
  // Forwarded to vtkPVGeometryFilter
  virtual void SetUseOutline(int);
  void SetTriangulate(int);
  void SetNonlinearSubdivisionLevel(int);
  virtual void SetGenerateFeatureEdges(bool);

  //***************************************************************************
  // Forwarded to vtkProperty.
  virtual void SetAmbientColor(double r, double g, double b);
  virtual void SetColor(double r, double g, double b);
  virtual void SetDiffuseColor(double r, double g, double b);
  virtual void SetEdgeColor(double r, double g, double b);
  virtual void SetInterpolation(int val);
  virtual void SetLineWidth(double val);
  virtual void SetOpacity(double val);
  virtual void SetPointSize(double val);
  virtual void SetSpecularColor(double r, double g, double b);
  virtual void SetSpecularPower(double val);
  virtual void SetLuminosity(double val);
  virtual void SetRenderPointsAsSpheres(bool);
  virtual void SetRenderLinesAsTubes(bool);

  //***************************************************************************
  // Forwarded to Actor.
  virtual void SetOrientation(double, double, double);
  virtual void SetOrigin(double, double, double);
  virtual void SetPickable(int val);
  virtual void SetPosition(double, double, double);
  virtual void SetScale(double, double, double);
  virtual void SetTexture(vtkTexture*);
  virtual void SetUserTransform(const double[16]);

  //***************************************************************************
  // Forwarded to Mapper and LODMapper.
  virtual void SetInterpolateScalarsBeforeMapping(int val);
  virtual void SetLookupTable(vtkScalarsToColors* val);
  //@{
  /**
   * Sets if scalars are mapped through a color-map or are used
   * directly as colors.
   * 0 maps to VTK_COLOR_MODE_DIRECT_SCALARS
   * 1 maps to VTK_COLOR_MODE_MAP_SCALARS
   * @see vtkScalarsToColors::MapScalars
   */
  virtual void SetMapScalars(int val);
  virtual void SetStatic(int val);
  //@}

  /**
   * Provides access to the actor used by this representation.
   */
  vtkPVLODActor* GetActor() { return this->GetRenderedProp(); }

  //@{
  /**
   * Set/get the visibility for a single block.
   */
  virtual void SetBlockVisibility(unsigned int index, bool visible);
  virtual bool GetBlockVisibility(unsigned int index) const;
  virtual void RemoveBlockVisibility(unsigned int index, bool = true);
  virtual void RemoveBlockVisibilities();
  //@}

  //@{
  /**
   * Set/get the color for a single block.
   */
  virtual void SetBlockColor(unsigned int index, double r, double g, double b);
  virtual void SetBlockColor(unsigned int index, double* color);
  virtual double* GetBlockColor(unsigned int index);
  virtual void RemoveBlockColor(unsigned int index);
  virtual void RemoveBlockColors();
  //@}

  //@{
  /**
   * Set/get the opacityfor a single block.
   */
  virtual void SetBlockOpacity(unsigned int index, double opacity);
  virtual void SetBlockOpacity(unsigned int index, double* opacity);
  virtual double GetBlockOpacity(unsigned int index);
  virtual void RemoveBlockOpacity(unsigned int index);
  virtual void RemoveBlockOpacities();
  //@}

  /**
   * Convenience method to get the array name used to scalar color with.
   */
  const char* GetColorArrayName();

  /**
   * Convenience method to get bounds from a dataset/composite dataset.
   * If a vtkCompositeDataDisplayAttributes \a cdAttributes is provided and
   * if the input data \a dataObject is vtkCompositeDataSet, only visible
   * blocks of the data will be used to compute the bounds.
   * Returns true if valid bounds were computed.
   */
  static bool GetBounds(
    vtkDataObject* dataObject, double bounds[6], vtkCompositeDataDisplayAttributes* cdAttributes);

  //@{
  /**
   * For OSPRay controls sizing of implicit spheres (points) and
   * cylinders (lines)
   */
  virtual void SetEnableScaling(int v);
  virtual void SetScalingArrayName(const char*);
  virtual void SetScalingFunction(vtkPiecewiseFunction* pwf);
  //@}

  /**
   * For OSPRay, choose from among available materials.
   */
  virtual void SetMaterial(const char*);

  //@{
  /**
   * Specify whether or not to redistribute the data. The default is false
   * since that is the only way in general to guarantee correct rendering.
   * Can set to true if all rendered data sets are based on the same
   * data partitioning in order to save on the data redistribution.
   */
  vtkSetMacro(UseDataPartitions, bool);
  vtkGetMacro(UseDataPartitions, bool);
  //@}

  //@{
  /**
   * Specify whether or not to shader replacements string must be used.
   */
  virtual void SetUseShaderReplacements(bool);
  vtkGetMacro(UseShaderReplacements, bool);
  //@}

  /**
   * Specify shader replacements using a Json string.
   * Please refer to the XML definition of the property for details about
   * the expected Json string format.
   */
  virtual void SetShaderReplacements(const char*);

protected:
  vtkGeometryRepresentation();
  ~vtkGeometryRepresentation() override;

  /**
   * This method is called in the constructor. If the subclasses override any of
   * the iVar vtkObject's of this class e.g. the Mappers, GeometryFilter etc.,
   * they should call this method again in their constructor. It must be totally
   * safe to call this method repeatedly.
   */
  virtual void SetupDefaults();

  /**
   * Fill input port information.
   */
  int FillInputPortInformation(int port, vtkInformation* info) VTK_OVERRIDE;

  /**
   * Subclasses should override this to connect inputs to the internal pipeline
   * as necessary. Since most representations are "meta-filters" (i.e. filters
   * containing other filters), you should create shallow copies of your input
   * before connecting to the internal pipeline. The convenience method
   * GetInternalOutputPort will create a cached shallow copy of a specified
   * input for you. The related helper functions GetInternalAnnotationOutputPort,
   * GetInternalSelectionOutputPort should be used to obtain a selection or
   * annotation port whose selections are localized for a particular input data object.
   */
  int RequestData(vtkInformation*, vtkInformationVector**, vtkInformationVector*) VTK_OVERRIDE;

  /**
   * Overridden to request correct ghost-level to avoid internal surfaces.
   */
  int RequestUpdateExtent(vtkInformation* request, vtkInformationVector** inputVector,
    vtkInformationVector* outputVector) VTK_OVERRIDE;

  /**
   * Adds the representation to the view.  This is called from
   * vtkView::AddRepresentation().  Subclasses should override this method.
   * Returns true if the addition succeeds.
   */
  bool AddToView(vtkView* view) VTK_OVERRIDE;

  /**
   * Removes the representation to the view.  This is called from
   * vtkView::RemoveRepresentation().  Subclasses should override this method.
   * Returns true if the removal succeeds.
   */
  bool RemoveFromView(vtkView* view) VTK_OVERRIDE;

  /**
   * Passes on parameters to vtkProperty and vtkMapper
   */
  virtual void UpdateColoringParameters();

  /**
   * Used in ConvertSelection to locate the prop used for actual rendering.
   */
  virtual vtkPVLODActor* GetRenderedProp() { return this->Actor; }

  /**
   * Overridden to check with the vtkPVCacheKeeper to see if the key is cached.
   */
  bool IsCached(double cache_key) VTK_OVERRIDE;

  // Progress Callback
  void HandleGeometryRepresentationProgress(vtkObject* caller, unsigned long, void*);

  /**
   * Block attributes in a mapper are referenced to each block through DataObject
   * pointers. Since DataObjects may change after updating the pipeline, this class
   * maintains an additional map using the flat-index as a key.  This method updates
   * the mapper's attributes with those cached in this representation; This is done
   * after the data has updated (multi-block nodes change after an update).
   */
  void UpdateBlockAttributes(vtkMapper* mapper);

  /**
   * Computes the bounds of the visible data based on the block visibilities in the
   * composite data attributes of the mapper.
   */
  void ComputeVisibleDataBounds();

  /**
   * Update the mapper with the shader replacement strings if feature is enabled.
   */
  void UpdateShaderReplacements();

  vtkAlgorithm* GeometryFilter;
  vtkAlgorithm* MultiBlockMaker;
  vtkPVCacheKeeper* CacheKeeper;
  vtkGeometryRepresentation_detail::DecimationFilterType* Decimator;
  vtkPVGeometryFilter* LODOutlineFilter;

  vtkMapper* Mapper;
  vtkMapper* LODMapper;
  vtkPVLODActor* Actor;
  vtkProperty* Property;

  double Ambient;
  double Specular;
  double Diffuse;
  int Representation;
  bool SuppressLOD;
  bool RequestGhostCellsIfNeeded;
  double VisibleDataBounds[6];

  vtkTimeStamp VisibleDataBoundsTime;

  vtkPiecewiseFunction* PWF;

  bool UseDataPartitions;

  bool UseShaderReplacements;
  std::string ShaderReplacementsString;

  bool BlockAttrChanged = false;
  vtkTimeStamp BlockAttributeTime;
  bool UpdateBlockAttrLOD = false;
  std::unordered_map<unsigned int, bool> BlockVisibilities;
  std::unordered_map<unsigned int, double> BlockOpacities;
  std::unordered_map<unsigned int, std::array<double, 3> > BlockColors;

private:
  vtkGeometryRepresentation(const vtkGeometryRepresentation&) = delete;
  void operator=(const vtkGeometryRepresentation&) = delete;
};

#endif
