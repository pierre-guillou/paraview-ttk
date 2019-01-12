/*=========================================================================

  Program:   ParaView
  Module:    vtkGeometryRepresentation.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkGeometryRepresentation.h"
#include "vtkGeometryRepresentationInternal.h"

#include "vtkAlgorithmOutput.h"
#include "vtkBoundingBox.h"
#include "vtkCallbackCommand.h"
#include "vtkCommand.h"
#include "vtkCompositeDataDisplayAttributes.h"
#include "vtkCompositeDataIterator.h"
#include "vtkCompositePolyDataMapper2.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMath.h"
#include "vtkMatrix4x4.h"
#include "vtkMultiBlockDataSet.h"
#include "vtkMultiBlockDataSetAlgorithm.h"
#include "vtkMultiProcessController.h"
#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkPVCacheKeeper.h"
#include "vtkPVConfig.h"
#include "vtkPVGeometryFilter.h"
#include "vtkPVLODActor.h"
#include "vtkPVRenderView.h"
#include "vtkPVTrivialProducer.h"
#include "vtkPVUpdateSuppressor.h"
#include "vtkPointData.h"
#include "vtkProcessModule.h"
#include "vtkProperty.h"
#include "vtkRenderer.h"
#include "vtkSelection.h"
#include "vtkSelectionConverter.h"
#include "vtkSelectionNode.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkTransform.h"
#include "vtkUnstructuredGrid.h"

#ifdef PARAVIEW_USE_OSPRAY
#include "vtkOSPRayActorNode.h"
#endif

#include <vtk_jsoncpp.h>
#include <vtksys/SystemTools.hxx>

#include <memory>
#include <tuple>
#include <vector>

//*****************************************************************************
// This is used to convert a vtkPolyData to a vtkMultiBlockDataSet. If input is
// vtkMultiBlockDataSet, then this is simply a pass-through filter. This makes
// it easier to unify the code to select and render data by simply dealing with
// vtkMultiBlockDataSet always.
class vtkGeometryRepresentationMultiBlockMaker : public vtkMultiBlockDataSetAlgorithm
{
public:
  static vtkGeometryRepresentationMultiBlockMaker* New();
  vtkTypeMacro(vtkGeometryRepresentationMultiBlockMaker, vtkMultiBlockDataSetAlgorithm);

protected:
  int RequestData(vtkInformation*, vtkInformationVector** inputVector,
    vtkInformationVector* outputVector) VTK_OVERRIDE
  {
    vtkMultiBlockDataSet* inputMB = vtkMultiBlockDataSet::GetData(inputVector[0], 0);
    vtkMultiBlockDataSet* outputMB = vtkMultiBlockDataSet::GetData(outputVector, 0);
    if (inputMB)
    {
      outputMB->ShallowCopy(inputMB);
      return 1;
    }

    vtkDataObject* inputDO = vtkDataObject::GetData(inputVector[0], 0);
    vtkDataObject* clone = inputDO->NewInstance();
    clone->ShallowCopy(inputDO);
    outputMB->SetBlock(0, clone);
    clone->Delete();
    return 1;
  }

  int FillInputPortInformation(int, vtkInformation* info) VTK_OVERRIDE
  {
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkPolyData");
    info->Append(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkMultiBlockDataSet");
    return 1;
  }
};
vtkStandardNewMacro(vtkGeometryRepresentationMultiBlockMaker);

//*****************************************************************************

vtkStandardNewMacro(vtkGeometryRepresentation);
//----------------------------------------------------------------------------

void vtkGeometryRepresentation::HandleGeometryRepresentationProgress(
  vtkObject* caller, unsigned long, void*)
{
  vtkAlgorithm* algorithm = vtkAlgorithm::SafeDownCast(caller);
  if (algorithm)
  {
    double progress = algorithm->GetProgress();
    if (progress > 0.0 && progress < 1.0)
    {
      if (algorithm == this->GeometryFilter)
      {
        this->UpdateProgress(progress * 0.8);
      }
      else if (algorithm == this->MultiBlockMaker)
      {
        this->UpdateProgress(0.8 + progress * 0.05);
      }
      else if (algorithm == this->CacheKeeper)
      {
        this->UpdateProgress(0.85 + progress * 0.05);
      }
      else if (algorithm == this->Decimator)
      {
        this->UpdateProgress(0.90 + progress * 0.05);
      }
      else if (algorithm == this->LODOutlineFilter)
      {
        this->UpdateProgress(0.95 + progress * 0.05);
      }
    }
    if (this->AbortExecute)
    {
      algorithm->SetAbortExecute(1);
    }
  }
}

vtkGeometryRepresentation::vtkGeometryRepresentation()
{
  this->GeometryFilter = vtkPVGeometryFilter::New();
  this->CacheKeeper = vtkPVCacheKeeper::New();
  this->MultiBlockMaker = vtkGeometryRepresentationMultiBlockMaker::New();
  this->Decimator = vtkGeometryRepresentation_detail::DecimationFilterType::New();
  this->LODOutlineFilter = vtkPVGeometryFilter::New();

  // connect progress bar
  this->GeometryFilter->AddObserver(vtkCommand::ProgressEvent, this,
    &vtkGeometryRepresentation::HandleGeometryRepresentationProgress);
  this->CacheKeeper->AddObserver(vtkCommand::ProgressEvent, this,
    &vtkGeometryRepresentation::HandleGeometryRepresentationProgress);
  this->MultiBlockMaker->AddObserver(vtkCommand::ProgressEvent, this,
    &vtkGeometryRepresentation::HandleGeometryRepresentationProgress);
  this->Decimator->AddObserver(vtkCommand::ProgressEvent, this,
    &vtkGeometryRepresentation::HandleGeometryRepresentationProgress);
  this->LODOutlineFilter->AddObserver(vtkCommand::ProgressEvent, this,
    &vtkGeometryRepresentation::HandleGeometryRepresentationProgress);

  // setup the selection mapper so that we don't need to make any selection
  // conversions after rendering.
  vtkCompositePolyDataMapper2* mapper = vtkCompositePolyDataMapper2::New();
  mapper->SetPointIdArrayName("vtkOriginalPointIds");
  mapper->SetCellIdArrayName("vtkOriginalCellIds");
  mapper->SetProcessIdArrayName("vtkProcessId");
  mapper->SetCompositeIdArrayName("vtkCompositeIndex");

  this->Mapper = mapper;
  this->LODMapper = vtkCompositePolyDataMapper2::New();
  this->Actor = vtkPVLODActor::New();
  this->Property = vtkProperty::New();

  // setup composite display attributes
  vtkCompositeDataDisplayAttributes* compositeAttributes = vtkCompositeDataDisplayAttributes::New();
  vtkCompositePolyDataMapper2::SafeDownCast(this->Mapper)
    ->SetCompositeDataDisplayAttributes(compositeAttributes);
  vtkCompositePolyDataMapper2::SafeDownCast(this->LODMapper)
    ->SetCompositeDataDisplayAttributes(compositeAttributes);
  compositeAttributes->Delete();

  this->RequestGhostCellsIfNeeded = true;
  this->Ambient = 0.0;
  this->Diffuse = 1.0;
  this->Specular = 0.0;
  this->Representation = SURFACE;

  this->SuppressLOD = false;

  vtkMath::UninitializeBounds(this->VisibleDataBounds);

  this->SetupDefaults();

  this->PWF = NULL;

  this->UseDataPartitions = false;

  this->UseShaderReplacements = false;
  this->ShaderReplacementsString = "";
}

//----------------------------------------------------------------------------
vtkGeometryRepresentation::~vtkGeometryRepresentation()
{
  this->CacheKeeper->Delete();
  this->GeometryFilter->Delete();
  this->MultiBlockMaker->Delete();
  this->Decimator->Delete();
  this->LODOutlineFilter->Delete();
  this->Mapper->Delete();
  this->LODMapper->Delete();
  this->Actor->Delete();
  this->Property->Delete();
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetupDefaults()
{
  this->Decimator->SetLODFactor(0.5);

  this->LODOutlineFilter->SetUseOutline(1);

  vtkPVGeometryFilter* geomFilter = vtkPVGeometryFilter::SafeDownCast(this->GeometryFilter);
  if (geomFilter)
  {
    geomFilter->SetUseOutline(0);
    geomFilter->SetTriangulate(0);
    geomFilter->SetNonlinearSubdivisionLevel(1);
    geomFilter->SetPassThroughCellIds(1);
    geomFilter->SetPassThroughPointIds(1);
  }

  this->MultiBlockMaker->SetInputConnection(this->GeometryFilter->GetOutputPort());
  this->CacheKeeper->SetInputConnection(this->MultiBlockMaker->GetOutputPort());
  this->Decimator->SetInputConnection(this->CacheKeeper->GetOutputPort());
  this->LODOutlineFilter->SetInputConnection(this->CacheKeeper->GetOutputPort());

  this->Actor->SetMapper(this->Mapper);
  this->Actor->SetLODMapper(this->LODMapper);
  this->Actor->SetProperty(this->Property);

  // Not insanely thrilled about this API on vtkProp about properties, but oh
  // well. We have to live with it.
  vtkInformation* keys = vtkInformation::New();
  this->Actor->SetPropertyKeys(keys);
  keys->Delete();
}

//----------------------------------------------------------------------------
int vtkGeometryRepresentation::GetBlockColorsDistinctValues()
{
  vtkPVGeometryFilter* geomFilter = vtkPVGeometryFilter::SafeDownCast(this->GeometryFilter);
  if (geomFilter)
  {
    return geomFilter->GetBlockColorsDistinctValues();
  }
  return 2;
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetBlockColorsDistinctValues(int distinctValues)
{
  vtkPVGeometryFilter* geomFilter = vtkPVGeometryFilter::SafeDownCast(this->GeometryFilter);
  if (geomFilter)
  {
    geomFilter->SetBlockColorsDistinctValues(distinctValues);
    this->MarkModified();
  }
}

//----------------------------------------------------------------------------
int vtkGeometryRepresentation::FillInputPortInformation(int vtkNotUsed(port), vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
  info->Append(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkCompositeDataSet");

  // Saying INPUT_IS_OPTIONAL() is essential, since representations don't have
  // any inputs on client-side (in client-server, client-render-server mode) and
  // render-server-side (in client-render-server mode).
  info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);

  return 1;
}

//----------------------------------------------------------------------------
int vtkGeometryRepresentation::ProcessViewRequest(
  vtkInformationRequestKey* request_type, vtkInformation* inInfo, vtkInformation* outInfo)
{
  if (!this->Superclass::ProcessViewRequest(request_type, inInfo, outInfo))
  {
    // i.e. this->GetVisibility() == false, hence nothing to do.
    return 0;
  }

  if (request_type == vtkPVView::REQUEST_UPDATE())
  {
    // provide the "geometry" to the view so the view can delivery it to the
    // rendering nodes as and when needed.
    // When this process doesn't have any valid input, the cache-keeper is setup
    // to provide a place-holder dataset of the right type. This is essential
    // since the vtkPVRenderView uses the type specified to decide on the
    // delivery mechanism, among other things.
    vtkPVRenderView::SetPiece(inInfo, this, this->CacheKeeper->GetOutputDataObject(0));

    // Since we are rendering polydata, it can be redistributed when ordered
    // compositing is needed. So let the view know that it can feel free to
    // redistribute data as and when needed.
    vtkPVRenderView::MarkAsRedistributable(inInfo, this);

    this->ComputeVisibleDataBounds();

    // Tell the view if this representation needs ordered compositing. We need
    // ordered compositing when rendering translucent geometry.
    if (this->Actor->HasTranslucentPolygonalGeometry())
    {
      // We need to extend this condition to consider translucent LUTs once we
      // start supporting them,

      outInfo->Set(vtkPVRenderView::NEED_ORDERED_COMPOSITING(), 1);
      // Pass partitioning information to the render view.
      if (this->UseDataPartitions == true)
      {
        vtkPVRenderView::SetOrderedCompositingInformation(inInfo, this->VisibleDataBounds);
      }
    }

    // Finally, let the view know about the geometry bounds. The view uses this
    // information for resetting camera and clip planes. Since this
    // representation allows users to transform the geometry, we need to ensure
    // that the bounds we report include the transformation as well.
    vtkNew<vtkMatrix4x4> matrix;
    this->Actor->GetMatrix(matrix.GetPointer());
    vtkPVRenderView::SetGeometryBounds(inInfo, this->VisibleDataBounds, matrix.GetPointer());
  }
  else if (request_type == vtkPVView::REQUEST_UPDATE_LOD())
  {
    // Called to generate and provide the LOD data to the view.
    // If SuppressLOD is true, we tell the view we have no LOD data to provide,
    // otherwise we provide the decimated data.
    if (!this->SuppressLOD)
    {
      if (inInfo->Has(vtkPVRenderView::USE_OUTLINE_FOR_LOD()))
      {
        // HACK to ensure that when Decimator is next employed, it delivers a
        // new geometry.
        this->Decimator->Modified();

        this->LODOutlineFilter->Update();
        // Pass along the LOD geometry to the view so that it can deliver it to
        // the rendering node as and when needed.
        vtkPVRenderView::SetPieceLOD(inInfo, this, this->LODOutlineFilter->GetOutputDataObject(0));
      }
      else
      {
        // HACK to ensure that when Decimator is next employed, it delivers a
        // new geometry.
        this->LODOutlineFilter->Modified();

        if (inInfo->Has(vtkPVRenderView::LOD_RESOLUTION()))
        {
          // We handle this number differently depending on decimator
          // implementation.
          const double factor = inInfo->Get(vtkPVRenderView::LOD_RESOLUTION());
          this->Decimator->SetLODFactor(factor);
        }

        this->Decimator->Update();

        // Pass along the LOD geometry to the view so that it can deliver it to
        // the rendering node as and when needed.
        vtkPVRenderView::SetPieceLOD(inInfo, this, this->Decimator->GetOutputDataObject(0));
      }
    }
  }
  else if (request_type == vtkPVView::REQUEST_RENDER())
  {
    vtkAlgorithmOutput* producerPort = vtkPVRenderView::GetPieceProducer(inInfo, this);
    vtkAlgorithmOutput* producerPortLOD = vtkPVRenderView::GetPieceProducerLOD(inInfo, this);
    this->Mapper->SetInputConnection(0, producerPort);
    this->LODMapper->SetInputConnection(0, producerPortLOD);

    // This is called just before the vtk-level render. In this pass, we simply
    // pick the correct rendering mode and rendering parameters.
    bool lod = this->SuppressLOD ? false : (inInfo->Has(vtkPVRenderView::USE_LOD()) == 1);
    this->Actor->SetEnableLOD(lod ? 1 : 0);
    this->UpdateColoringParameters();

    auto data = producerPort->GetProducer()->GetOutputDataObject(0);
    if (this->BlockAttributeTime < data->GetMTime() || this->BlockAttrChanged)
    {
      this->UpdateBlockAttributes(this->Mapper);
      this->BlockAttributeTime.Modified();
      this->BlockAttrChanged = false;

      // This flag makes the following LOD render requests to also update the block
      // attribute state, if there were any changes.
      this->UpdateBlockAttrLOD = true;
    }

    // Make following (LOD) render request to update.
    if (lod && this->UpdateBlockAttrLOD)
    {
      this->UpdateBlockAttributes(this->LODMapper);
      this->UpdateBlockAttrLOD = false;
    }
  }

  return 1;
}

//----------------------------------------------------------------------------
#if !defined(VTK_LEGACY_REMOVE)
bool vtkGeometryRepresentation::DoRequestGhostCells(vtkInformation* info)
{
  return (vtkProcessModule::GetNumberOfGhostLevelsToRequest(info) > 0);
}
#endif

//----------------------------------------------------------------------------
int vtkGeometryRepresentation::RequestUpdateExtent(
  vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  this->Superclass::RequestUpdateExtent(request, inputVector, outputVector);

  // ensure that the ghost-level information is setup correctly to avoid
  // internal faces for unstructured grids.
  for (int cc = 0; cc < this->GetNumberOfInputPorts(); cc++)
  {
    for (int kk = 0; kk < inputVector[cc]->GetNumberOfInformationObjects(); kk++)
    {
      vtkInformation* inInfo = inputVector[cc]->GetInformationObject(kk);
      int ghostLevels =
        inInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_NUMBER_OF_GHOST_LEVELS());
      if (this->RequestGhostCellsIfNeeded)
      {
        ghostLevels += vtkProcessModule::GetNumberOfGhostLevelsToRequest(inInfo);
      }
      inInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_NUMBER_OF_GHOST_LEVELS(), ghostLevels);
    }
  }

  return 1;
}

//----------------------------------------------------------------------------
int vtkGeometryRepresentation::RequestData(
  vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{

  // Pass caching information to the cache keeper.
  this->CacheKeeper->SetCachingEnabled(this->GetUseCache());
  this->CacheKeeper->SetCacheTime(this->GetCacheKey());
  // cout << this << ": Using Cache (" << this->GetCacheKey() << ") : is_cached = " <<
  //  this->IsCached(this->GetCacheKey()) << " && use_cache = " <<  this->GetUseCache() << endl;

  if (inputVector[0]->GetNumberOfInformationObjects() == 1)
  {
    vtkInformation* inInfo = inputVector[0]->GetInformationObject(0);
    if (inInfo->Has(vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT()))
    {
      vtkAlgorithmOutput* aout = this->GetInternalOutputPort();
      vtkPVTrivialProducer* prod = vtkPVTrivialProducer::SafeDownCast(aout->GetProducer());
      if (prod)
      {
        prod->SetWholeExtent(inInfo->Get(vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT()));
      }
    }
    this->GeometryFilter->SetInputConnection(this->GetInternalOutputPort());
  }
  else
  {
    vtkNew<vtkMultiBlockDataSet> placeholder;
    this->GeometryFilter->SetInputDataObject(0, placeholder.GetPointer());
  }
  this->CacheKeeper->Update();

  // HACK: To overcome issue with PolyDataMapper (OpenGL2). It doesn't recreate
  // VBO/IBOs when using data from cache. I suspect it's because the blocks in
  // the MB dataset have older MTime.
  this->Mapper->Modified();

  return this->Superclass::RequestData(request, inputVector, outputVector);
}

//----------------------------------------------------------------------------
bool vtkGeometryRepresentation::GetBounds(
  vtkDataObject* dataObject, double bounds[6], vtkCompositeDataDisplayAttributes* cdAttributes)
{
  vtkMath::UninitializeBounds(bounds);
  if (vtkCompositeDataSet* cd = vtkCompositeDataSet::SafeDownCast(dataObject))
  {
    // computing bounds with only visible blocks
    vtkCompositeDataDisplayAttributes::ComputeVisibleBounds(cdAttributes, cd, bounds);
    if (vtkBoundingBox::IsValid(bounds))
    {
      return true;
    }
  }
  else if (vtkDataSet* ds = vtkDataSet::SafeDownCast(dataObject))
  {
    ds->GetBounds(bounds);
    return (vtkMath::AreBoundsInitialized(bounds) == 1);
  }
  return false;
}

//----------------------------------------------------------------------------
bool vtkGeometryRepresentation::IsCached(double cache_key)
{
  return this->CacheKeeper->IsCached(cache_key);
}

//----------------------------------------------------------------------------
vtkDataObject* vtkGeometryRepresentation::GetRenderedDataObject(int port)
{
  (void)port;
  if (this->GeometryFilter->GetNumberOfInputConnections(0) > 0)
  {
    return this->CacheKeeper->GetOutputDataObject(0);
  }
  return NULL;
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::MarkModified()
{
  if (!this->GetUseCache())
  {
    // Cleanup caches when not using cache.
    this->CacheKeeper->RemoveAllCaches();
  }
  this->Superclass::MarkModified();
}

//----------------------------------------------------------------------------
bool vtkGeometryRepresentation::AddToView(vtkView* view)
{
  vtkPVRenderView* rview = vtkPVRenderView::SafeDownCast(view);
  if (rview)
  {
    rview->GetRenderer()->AddActor(this->Actor);

    // Indicate that this is prop that we are rendering when hardware selection
    // is enabled.
    rview->RegisterPropForHardwareSelection(this, this->GetRenderedProp());
    return this->Superclass::AddToView(view);
  }
  return false;
}

//----------------------------------------------------------------------------
bool vtkGeometryRepresentation::RemoveFromView(vtkView* view)
{
  vtkPVRenderView* rview = vtkPVRenderView::SafeDownCast(view);
  if (rview)
  {
    rview->GetRenderer()->RemoveActor(this->Actor);
    rview->UnRegisterPropForHardwareSelection(this, this->GetRenderedProp());
    return this->Superclass::RemoveFromView(view);
  }
  return false;
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetRepresentation(const char* type)
{
  if (vtksys::SystemTools::Strucmp(type, "Points") == 0)
  {
    this->SetRepresentation(POINTS);
  }
  else if (vtksys::SystemTools::Strucmp(type, "Wireframe") == 0)
  {
    this->SetRepresentation(WIREFRAME);
  }
  else if (vtksys::SystemTools::Strucmp(type, "Surface") == 0)
  {
    this->SetRepresentation(SURFACE);
  }
  else if (vtksys::SystemTools::Strucmp(type, "Surface With Edges") == 0)
  {
    this->SetRepresentation(SURFACE_WITH_EDGES);
  }
  else
  {
    vtkErrorMacro("Invalid type: " << type);
  }
}

//----------------------------------------------------------------------------
const char* vtkGeometryRepresentation::GetColorArrayName()
{
  vtkInformation* info = this->GetInputArrayInformation(0);
  if (info && info->Has(vtkDataObject::FIELD_ASSOCIATION()) &&
    info->Has(vtkDataObject::FIELD_NAME()))
  {
    return info->Get(vtkDataObject::FIELD_NAME());
  }
  return NULL;
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::UpdateColoringParameters()
{
  bool using_scalar_coloring = false;

  vtkInformation* info = this->GetInputArrayInformation(0);
  if (info && info->Has(vtkDataObject::FIELD_ASSOCIATION()) &&
    info->Has(vtkDataObject::FIELD_NAME()))
  {
    const char* colorArrayName = info->Get(vtkDataObject::FIELD_NAME());
    int fieldAssociation = info->Get(vtkDataObject::FIELD_ASSOCIATION());
    if (colorArrayName && colorArrayName[0])
    {
      this->Mapper->SetScalarVisibility(1);
      this->LODMapper->SetScalarVisibility(1);
      this->Mapper->SelectColorArray(colorArrayName);
      this->LODMapper->SelectColorArray(colorArrayName);
      this->Mapper->SetUseLookupTableScalarRange(1);
      this->LODMapper->SetUseLookupTableScalarRange(1);
      switch (fieldAssociation)
      {
        case vtkDataObject::FIELD_ASSOCIATION_CELLS:
          this->Mapper->SetScalarMode(VTK_SCALAR_MODE_USE_CELL_FIELD_DATA);
          this->LODMapper->SetScalarMode(VTK_SCALAR_MODE_USE_CELL_FIELD_DATA);
          break;

        case vtkDataObject::FIELD_ASSOCIATION_NONE:
          this->Mapper->SetScalarMode(VTK_SCALAR_MODE_USE_FIELD_DATA);
          // Color entire block by zeroth tuple in the field data
          this->Mapper->SetFieldDataTupleId(0);
          this->LODMapper->SetScalarMode(VTK_SCALAR_MODE_USE_FIELD_DATA);
          this->LODMapper->SetFieldDataTupleId(0);
          break;

        case vtkDataObject::FIELD_ASSOCIATION_POINTS:
        default:
          this->Mapper->SetScalarMode(VTK_SCALAR_MODE_USE_POINT_FIELD_DATA);
          this->LODMapper->SetScalarMode(VTK_SCALAR_MODE_USE_POINT_FIELD_DATA);
          break;
      }
      using_scalar_coloring = true;
    }
  }

  if (!using_scalar_coloring)
  {
    this->Mapper->SetScalarVisibility(0);
    this->LODMapper->SetScalarVisibility(0);
    const char* null = NULL;
    this->Mapper->SelectColorArray(null);
    this->LODMapper->SelectColorArray(null);
  }

  // Adjust material properties.
  double diffuse = this->Diffuse;
  double specular = this->Specular;
  double ambient = this->Ambient;

  if (this->Representation != SURFACE && this->Representation != SURFACE_WITH_EDGES)
  {
    if ((this->Representation == WIREFRAME && this->Property->GetRenderLinesAsTubes()) ||
      (this->Representation == POINTS && this->Property->GetRenderPointsAsSpheres()))
    {
      // use diffuse lighting, since we're rendering as tubes or spheres.
    }
    else
    {
      diffuse = 0.0;
      ambient = 1.0;
    }
  }

  this->Property->SetAmbient(ambient);
  this->Property->SetSpecular(specular);
  this->Property->SetDiffuse(diffuse);

  switch (this->Representation)
  {
    case SURFACE_WITH_EDGES:
      this->Property->SetEdgeVisibility(1);
      this->Property->SetRepresentation(VTK_SURFACE);
      break;

    default:
      this->Property->SetEdgeVisibility(0);
      this->Property->SetRepresentation(this->Representation);
  }
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetVisibility(bool val)
{
  this->Actor->SetVisibility(val);
  this->Superclass::SetVisibility(val);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//****************************************************************************
// Methods merely forwarding parameters to internal objects.
//****************************************************************************

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetLookupTable(vtkScalarsToColors* val)
{
  this->Mapper->SetLookupTable(val);
  this->LODMapper->SetLookupTable(val);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetMapScalars(int val)
{
  if (val < 0 || val > 1)
  {
    vtkWarningMacro(<< "Invalid parameter for vtkGeometryRepresentation::SetMapScalars: " << val);
    val = 0;
  }
  int mapToColorMode[] = { VTK_COLOR_MODE_DIRECT_SCALARS, VTK_COLOR_MODE_MAP_SCALARS };
  this->Mapper->SetColorMode(mapToColorMode[val]);
  this->LODMapper->SetColorMode(mapToColorMode[val]);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetInterpolateScalarsBeforeMapping(int val)
{
  this->Mapper->SetInterpolateScalarsBeforeMapping(val);
  this->LODMapper->SetInterpolateScalarsBeforeMapping(val);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetStatic(int val)
{
  this->Mapper->SetStatic(val);
  this->LODMapper->SetStatic(val);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetColor(double r, double g, double b)
{
  this->Property->SetColor(r, g, b);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetLineWidth(double val)
{
  this->Property->SetLineWidth(val);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetOpacity(double val)
{
  this->Property->SetOpacity(val);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetLuminosity(double val)
{
#ifdef PARAVIEW_USE_OSPRAY
  vtkOSPRayActorNode::SetLuminosity(val, this->Property);
#else
  (void)val;
#endif
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetRenderPointsAsSpheres(bool val)
{
  this->Property->SetRenderPointsAsSpheres(val);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetRenderLinesAsTubes(bool val)
{
  this->Property->SetRenderLinesAsTubes(val);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetPointSize(double val)
{
  this->Property->SetPointSize(val);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetAmbientColor(double r, double g, double b)
{
  this->Property->SetAmbientColor(r, g, b);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetDiffuseColor(double r, double g, double b)
{
  this->Property->SetDiffuseColor(r, g, b);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetEdgeColor(double r, double g, double b)
{
  this->Property->SetEdgeColor(r, g, b);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetInterpolation(int val)
{
  this->Property->SetInterpolation(val);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetSpecularColor(double r, double g, double b)
{
  this->Property->SetSpecularColor(r, g, b);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetSpecularPower(double val)
{
  this->Property->SetSpecularPower(val);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetOrientation(double x, double y, double z)
{
  this->Actor->SetOrientation(x, y, z);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetOrigin(double x, double y, double z)
{
  this->Actor->SetOrigin(x, y, z);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetPickable(int val)
{
  this->Actor->SetPickable(val);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetPosition(double x, double y, double z)
{
  this->Actor->SetPosition(x, y, z);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetScale(double x, double y, double z)
{
  this->Actor->SetScale(x, y, z);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetUserTransform(const double matrix[16])
{
  vtkNew<vtkTransform> transform;
  transform->SetMatrix(matrix);
  this->Actor->SetUserTransform(transform.GetPointer());
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetTexture(vtkTexture* val)
{
  this->Actor->SetTexture(val);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetUseOutline(int val)
{
  if (vtkPVGeometryFilter::SafeDownCast(this->GeometryFilter))
  {
    vtkPVGeometryFilter::SafeDownCast(this->GeometryFilter)->SetUseOutline(val);
  }

  // since geometry filter needs to execute, we need to mark the representation
  // modified.
  this->MarkModified();
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetTriangulate(int val)
{
  if (vtkPVGeometryFilter::SafeDownCast(this->GeometryFilter))
  {
    vtkPVGeometryFilter::SafeDownCast(this->GeometryFilter)->SetTriangulate(val);
  }

  // since geometry filter needs to execute, we need to mark the representation
  // modified.
  this->MarkModified();
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetNonlinearSubdivisionLevel(int val)
{
  if (vtkPVGeometryFilter::SafeDownCast(this->GeometryFilter))
  {
    vtkPVGeometryFilter::SafeDownCast(this->GeometryFilter)->SetNonlinearSubdivisionLevel(val);
  }

  // since geometry filter needs to execute, we need to mark the representation
  // modified.
  this->MarkModified();
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetGenerateFeatureEdges(bool val)
{
  if (vtkPVGeometryFilter::SafeDownCast(this->GeometryFilter))
  {
    vtkPVGeometryFilter::SafeDownCast(this->GeometryFilter)->SetGenerateFeatureEdges(val);
  }

  // since geometry filter needs to execute, we need to mark the representation
  // modified.
  this->MarkModified();
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetBlockVisibility(unsigned int index, bool visible)
{
  this->BlockVisibilities[index] = visible;
  this->BlockAttrChanged = true;
}

//----------------------------------------------------------------------------
bool vtkGeometryRepresentation::GetBlockVisibility(unsigned int index) const
{
  auto it = this->BlockVisibilities.find(index);
  if (it == this->BlockVisibilities.cend())
  {
    return true;
  }
  return it->second;
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::RemoveBlockVisibility(unsigned int index, bool)
{
  auto it = this->BlockVisibilities.find(index);
  if (it == this->BlockVisibilities.cend())
  {
    return;
  }
  this->BlockVisibilities.erase(it);
  this->BlockAttrChanged = true;
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::RemoveBlockVisibilities()
{
  this->BlockVisibilities.clear();
  this->BlockAttrChanged = true;
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetBlockColor(unsigned int index, double r, double g, double b)
{
  std::array<double, 3> color = { { r, g, b } };
  this->BlockColors[index] = color;
  this->BlockAttrChanged = true;
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetBlockColor(unsigned int index, double* color)
{
  if (color)
  {
    this->SetBlockColor(index, color[0], color[1], color[2]);
  }
}

//----------------------------------------------------------------------------
double* vtkGeometryRepresentation::GetBlockColor(unsigned int index)
{
  auto it = this->BlockColors.find(index);
  if (it == this->BlockColors.cend())
  {
    return nullptr;
  }
  return it->second.data();
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::RemoveBlockColor(unsigned int index)
{
  auto it = this->BlockColors.find(index);
  if (it == this->BlockColors.cend())
  {
    return;
  }
  this->BlockColors.erase(it);
  this->BlockAttrChanged = true;
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::RemoveBlockColors()
{
  this->BlockColors.clear();
  this->BlockAttrChanged = true;
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetBlockOpacity(unsigned int index, double opacity)
{
  this->BlockOpacities[index] = opacity;
  this->BlockAttrChanged = true;
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetBlockOpacity(unsigned int index, double* opacity)
{
  if (opacity)
  {
    this->SetBlockOpacity(index, *opacity);
  }
}

//----------------------------------------------------------------------------
double vtkGeometryRepresentation::GetBlockOpacity(unsigned int index)
{
  auto it = this->BlockOpacities.find(index);
  if (it == this->BlockOpacities.cend())
  {
    return 0.0;
  }
  return it->second;
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::RemoveBlockOpacity(unsigned int index)
{
  auto it = this->BlockOpacities.find(index);
  if (it == this->BlockOpacities.cend())
  {
    return;
  }
  this->BlockOpacities.erase(it);
  this->BlockAttrChanged = true;
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::RemoveBlockOpacities()
{
  this->BlockOpacities.clear();
  this->BlockAttrChanged = true;
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::UpdateBlockAttributes(vtkMapper* mapper)
{
  auto cpm = vtkCompositePolyDataMapper2::SafeDownCast(mapper);
  if (!cpm)
  {
    vtkErrorMacro(<< "Invalid mapper!");
    return;
  }

  cpm->RemoveBlockVisibilities();
  for (auto const& item : this->BlockVisibilities)
  {
    cpm->SetBlockVisibility(item.first, item.second);
  }

  cpm->RemoveBlockColors();
  for (auto const& item : this->BlockColors)
  {
    auto& arr = item.second;
    double color[3] = { arr[0], arr[1], arr[2] };
    cpm->SetBlockColor(item.first, color);
  }

  cpm->RemoveBlockOpacities();
  for (auto const& item : this->BlockOpacities)
  {
    cpm->SetBlockOpacity(item.first, item.second);
  }
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetEnableScaling(int val)
{
#ifdef PARAVIEW_USE_OSPRAY
  this->Actor->SetEnableScaling(val);
#else
  (void)val;
#endif
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetScalingArrayName(const char* val)
{
#ifdef PARAVIEW_USE_OSPRAY
  this->Actor->SetScalingArrayName(val);
#else
  (void)val;
#endif
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetScalingFunction(vtkPiecewiseFunction* pwf)
{
#ifdef PARAVIEW_USE_OSPRAY
  this->Actor->SetScalingFunction(pwf);
#else
  (void)pwf;
#endif
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetMaterial(const char* val)
{
#ifdef PARAVIEW_USE_OSPRAY
  if (!strcmp(val, "None"))
  {
    this->Property->SetMaterialName(nullptr);
  }
  else
  {
    this->Property->SetMaterialName(val);
  }
#else
  (void)val;
#endif
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::ComputeVisibleDataBounds()
{
  if (this->VisibleDataBoundsTime < this->GetPipelineDataTime() ||
    (this->BlockAttrChanged && this->VisibleDataBoundsTime < this->BlockAttributeTime))
  {
    vtkDataObject* dataObject = this->CacheKeeper->GetOutputDataObject(0);
    vtkNew<vtkCompositeDataDisplayAttributes> cdAttributes;
    // If the input data is a composite dataset, use the currently set values for block
    // visibility rather than the cached ones from the last render.  This must be computed
    // in the REQUEST_UPDATE pass but the data is only copied to the mapper in the
    // REQUEST_RENDER pass.  This constructs a dummy vtkCompositeDataDisplayAttributes
    // with only the visibilities set and calls the helper function to compute the visible
    // bounds with that.
    if (vtkCompositeDataSet* cd = vtkCompositeDataSet::SafeDownCast(dataObject))
    {
      auto iter = vtkSmartPointer<vtkCompositeDataIterator>::Take(cd->NewIterator());
      for (iter->InitTraversal(); !iter->IsDoneWithTraversal(); iter->GoToNextItem())
      {
        auto visible = this->BlockVisibilities.find(iter->GetCurrentFlatIndex());
        if (visible != this->BlockVisibilities.end())
        {
          cdAttributes->SetBlockVisibility(iter->GetCurrentDataObject(), visible->second);
        }
      }
    }
    this->GetBounds(dataObject, this->VisibleDataBounds, cdAttributes);
    this->VisibleDataBoundsTime.Modified();
  }
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetUseShaderReplacements(bool useShaderRepl)
{
  if (this->UseShaderReplacements != useShaderRepl)
  {
    this->UseShaderReplacements = useShaderRepl;
    this->Modified();
    this->UpdateShaderReplacements();
  }
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetShaderReplacements(const char* replacementsString)
{
  if (strcmp(replacementsString, this->ShaderReplacementsString.c_str()))
  {
    this->ShaderReplacementsString = std::string(replacementsString);
    this->Modified();
    this->UpdateShaderReplacements();
  }
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::UpdateShaderReplacements()
{
  vtkOpenGLPolyDataMapper* glMapper = vtkOpenGLPolyDataMapper::SafeDownCast(this->Mapper);
  vtkOpenGLPolyDataMapper* glLODMapper = vtkOpenGLPolyDataMapper::SafeDownCast(this->LODMapper);

  if (!glMapper || !glLODMapper)
  {
    return;
  }

  glMapper->ClearAllShaderReplacements();
  glLODMapper->ClearAllShaderReplacements();

  if (!this->UseShaderReplacements || this->ShaderReplacementsString == "")
  {
    return;
  }

  Json::CharReaderBuilder builder;
  builder["collectComments"] = false;
  Json::Value root;
  std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  bool success = reader->parse(this->ShaderReplacementsString.c_str(),
    this->ShaderReplacementsString.c_str() + this->ShaderReplacementsString.length(), &root,
    nullptr);
  if (!success)
  {
    vtkGenericWarningMacro("Unable to parse the replacement Json string!");
    return;
  }
  bool isArray = root.isArray();
  size_t nbReplacements = isArray ? root.size() : 1;

  std::vector<std::tuple<vtkShader::Type, std::string, std::string> > replacements;
  for (size_t index = 0; index < nbReplacements; ++index)
  {
    const Json::Value& repl = isArray ? root[(int)index] : root;
    if (!repl.isMember("type"))
    {
      vtkErrorMacro("Syntax error in shader replacements: a type is required.");
      return;
    }
    std::string type = repl["type"].asString();
    vtkShader::Type shaderType = vtkShader::Unknown;
    if (type == "fragment")
    {
      shaderType = vtkShader::Fragment;
    }
    else if (type == "vertex")
    {
      shaderType = vtkShader::Vertex;
    }
    else if (type == "geometry")
    {
      shaderType = vtkShader::Geometry;
    }
    if (shaderType == vtkShader::Unknown)
    {
      vtkErrorMacro("Unknown shader type for replacement:" << type);
      return;
    }

    if (!repl.isMember("original"))
    {
      vtkErrorMacro("Syntax error in shader replacements: an original pattern is required.");
      return;
    }
    std::string original = repl["original"].asString();
    if (!repl.isMember("replacement"))
    {
      vtkErrorMacro("Syntax error in shader replacements: a replacement pattern is required.");
      return;
    }
    std::string replacement = repl["replacement"].asString();
    replacements.push_back(std::make_tuple(shaderType, original, replacement));
  }

  for (const auto& r : replacements)
  {
    glMapper->AddShaderReplacement(std::get<0>(r), std::get<1>(r), true, std::get<2>(r), true);
    glLODMapper->AddShaderReplacement(std::get<0>(r), std::get<1>(r), true, std::get<2>(r), true);
  }
}
