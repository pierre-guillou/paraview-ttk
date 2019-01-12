#include "vtkPVExtractBagPlots.h"

#include "vtkAbstractArray.h"
#include "vtkDoubleArray.h"
#include "vtkExtractFunctionalBagPlot.h"
#include "vtkHighestDensityRegionsStatistics.h"
#include "vtkImageData.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMultiBlockDataSet.h"
#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkPCAStatistics.h"
#include "vtkPSciVizPCAStats.h"
#include "vtkPointData.h"
#include "vtkStringArray.h"
#include "vtkTable.h"
#include "vtkTransposeTable.h"

#include <algorithm>
#include <set>
#include <sstream>
#include <string>

//----------------------------------------------------------------------------
// Internal class that holds selected columns
class PVExtractBagPlotsInternal
{
public:
  bool Clear()
  {
    if (this->Columns.empty())
    {
      return false;
    }
    this->Columns.clear();
    return true;
  }

  bool Has(const std::string& v) { return this->Columns.find(v) != this->Columns.end(); }

  bool Set(const std::string& v)
  {
    if (this->Has(v))
    {
      return false;
    }
    this->Columns.insert(v);
    return true;
  }

  std::set<std::string> Columns;
};

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkPVExtractBagPlots);

//----------------------------------------------------------------------------
vtkPVExtractBagPlots::vtkPVExtractBagPlots()
{
  this->TransposeTable = true;
  this->RobustPCA = false;
  this->KernelWidth = 1.;
  this->UseSilvermanRule = false;
  this->GridSize = 100;
  this->UserQuantile = 95;
  this->Internal = new PVExtractBagPlotsInternal();

  this->SetNumberOfOutputPorts(1);
}

//----------------------------------------------------------------------------
vtkPVExtractBagPlots::~vtkPVExtractBagPlots()
{
  delete this->Internal;
}

//----------------------------------------------------------------------------
int vtkPVExtractBagPlots::FillInputPortInformation(int vtkNotUsed(port), vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkTable");
  return 1;
}

//----------------------------------------------------------------------------
void vtkPVExtractBagPlots::EnableAttributeArray(const char* arrName)
{
  if (arrName)
  {
    if (this->Internal->Set(arrName))
    {
      this->Modified();
    }
  }
}

//----------------------------------------------------------------------------
void vtkPVExtractBagPlots::ClearAttributeArrays()
{
  if (this->Internal->Clear())
  {
    this->Modified();
  }
}

//----------------------------------------------------------------------------
void vtkPVExtractBagPlots::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  os << "TransposeTable: " << this->TransposeTable << std::endl;
  os << "RobustPCA: " << this->RobustPCA << std::endl;
  os << "KernelWidth: " << this->KernelWidth << std::endl;
  os << "UseSilvermanRule: " << this->UseSilvermanRule << std::endl;
  os << "GridSize: " << this->GridSize << std::endl;
  os << "UserQuantile: " << this->UserQuantile << std::endl;
}

// ----------------------------------------------------------------------
void vtkPVExtractBagPlots::GetEigenvalues(
  vtkMultiBlockDataSet* outputMetaDS, vtkDoubleArray* eigenvalues)
{
  vtkTable* outputMeta = vtkTable::SafeDownCast(outputMetaDS->GetBlock(1));

  if (!outputMeta)
  {
    vtkErrorMacro(<< "NULL table pointer!");
    return;
  }

  vtkDoubleArray* meanCol = vtkDoubleArray::SafeDownCast(outputMeta->GetColumnByName("Mean"));
  vtkStringArray* rowNames = vtkStringArray::SafeDownCast(outputMeta->GetColumnByName("Column"));

  eigenvalues->SetNumberOfComponents(1);

  // Get values
  for (vtkIdType i = 0, eval = 0; i < meanCol->GetNumberOfTuples(); i++)
  {
    std::stringstream ss;
    ss << "PCA " << eval;

    std::string rowName = rowNames->GetValue(i);
    if (rowName.compare(ss.str()) == 0)
    {
      eigenvalues->InsertNextValue(meanCol->GetValue(i));
      eval++;
    }
  }
}

// ----------------------------------------------------------------------
void vtkPVExtractBagPlots::GetEigenvectors(
  vtkMultiBlockDataSet* outputMetaDS, vtkDoubleArray* eigenvectors, vtkDoubleArray* eigenvalues)
{
  // Count eigenvalues
  this->GetEigenvalues(outputMetaDS, eigenvalues);
  vtkIdType numberOfEigenvalues = eigenvalues->GetNumberOfTuples();

  if (!outputMetaDS)
  {
    vtkErrorMacro(<< "NULL dataset pointer!");
  }

  vtkTable* outputMeta = vtkTable::SafeDownCast(outputMetaDS->GetBlock(1));

  if (!outputMeta)
  {
    vtkErrorMacro(<< "NULL table pointer!");
  }

  vtkDoubleArray* meanCol = vtkDoubleArray::SafeDownCast(outputMeta->GetColumnByName("Mean"));
  vtkStringArray* rowNames = vtkStringArray::SafeDownCast(outputMeta->GetColumnByName("Column"));

  eigenvectors->SetNumberOfComponents(numberOfEigenvalues);

  // Get vectors
  for (vtkIdType i = 0, eval = 0; i < meanCol->GetNumberOfTuples(); i++)
  {
    std::stringstream ss;
    ss << "PCA " << eval;

    std::string rowName = rowNames->GetValue(i);
    if (rowName.compare(ss.str()) == 0)
    {
      std::vector<double> eigenvector;
      for (int val = 0; val < numberOfEigenvalues; val++)
      {
        // The first two columns will always be "Column" and "Mean",
        // so start with the next one
        vtkDoubleArray* currentCol = vtkDoubleArray::SafeDownCast(outputMeta->GetColumn(val + 2));
        eigenvector.push_back(currentCol->GetValue(i));
      }

      eigenvectors->InsertNextTypedTuple(&eigenvector.front());
      eval++;
    }
  }
}

//----------------------------------------------------------------------------
int vtkPVExtractBagPlots::RequestData(
  vtkInformation*, vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  vtkInformation* outputInfo = outputVector->GetInformationObject(0);

  vtkTable* inTable = vtkTable::GetData(inputVector[0]);
  vtkMultiBlockDataSet* outTables =
    vtkMultiBlockDataSet::SafeDownCast(outputInfo->Get(vtkDataObject::DATA_OBJECT()));

  vtkTable* outTable = 0;
  vtkTable* outTable2 = 0;

  if (inTable->GetNumberOfColumns() == 0)
  {
    return 1;
  }

  if (!outTables)
  {
    return 0;
  }
  outTables->SetNumberOfBlocks(2);

  vtkNew<vtkTransposeTable> transpose;

  // Construct a table that holds only the selected columns
  vtkNew<vtkTable> subTable;
  std::set<std::string>::iterator iter = this->Internal->Columns.begin();
  for (; iter != this->Internal->Columns.end(); ++iter)
  {
    if (vtkAbstractArray* arr = inTable->GetColumnByName(iter->c_str()))
    {
      subTable->AddColumn(arr);
    }
  }

  vtkTable* inputTable = subTable.GetPointer();

  outTable = subTable.GetPointer();

  if (this->TransposeTable)
  {
    transpose->SetInputData(subTable.GetPointer());
    transpose->SetAddIdColumn(true);
    transpose->SetIdColumnName("ColName");
    transpose->Update();

    inputTable = transpose->GetOutput();
  }

  outTable2 = inputTable;

  // Compute the PCA on the provided input functions
  vtkNew<vtkPSciVizPCAStats> pca;
  pca->SetInputData(inputTable);
  pca->SetAttributeMode(vtkDataObject::ROW);
  for (vtkIdType i = 0; i < inputTable->GetNumberOfColumns(); i++)
  {
    vtkAbstractArray* arr = inputTable->GetColumn(i);
    if (strcmp(arr->GetName(), "ColName"))
    {
      pca->EnableAttributeArray(arr->GetName());
    }
  }

  pca->SetBasisScheme(vtkPCAStatistics::FIXED_BASIS_SIZE);
  pca->SetFixedBasisSize(2);
  pca->SetTrainingFraction(1.0);
  pca->SetRobustPCA(this->RobustPCA);
  pca->Update();

  vtkTable* outputPCATable =
    vtkTable::SafeDownCast(pca->GetOutputDataObject(vtkStatisticsAlgorithm::OUTPUT_MODEL));

  outTable2 = outputPCATable;

  vtkMultiBlockDataSet* outputMetaDS = vtkMultiBlockDataSet::SafeDownCast(
    pca->GetOutputDataObject(vtkStatisticsAlgorithm::OUTPUT_DATA));

  // Compute the explained variance
  vtkNew<vtkDoubleArray> eigenVectors;
  vtkNew<vtkDoubleArray> eigenValues;
  this->GetEigenvectors(outputMetaDS, eigenVectors.Get(), eigenValues.Get());

  double sumOfEigenValues = 0;
  for (vtkIdType i = 0; i < eigenValues->GetNumberOfTuples(); i++)
  {
    sumOfEigenValues += eigenValues->GetValue(i);
  }
  double explainedVariance =
    100. * ((eigenValues->GetValue(0) + eigenValues->GetValue(1)) / sumOfEigenValues);

  // Compute HDR
  vtkNew<vtkHighestDensityRegionsStatistics> hdr;
  hdr->SetInputData(vtkStatisticsAlgorithm::INPUT_DATA, outputPCATable);

  // Fetch and rename the 2 PCA coordinates components arrays
  vtkDataArray* xArray = NULL;
  vtkDataArray* yArray = NULL;
  for (vtkIdType i = 0; i < outputPCATable->GetNumberOfColumns(); i++)
  {
    vtkAbstractArray* arr = outputPCATable->GetColumn(i);
    char* str = arr->GetName();
    if (strstr(str, "PCA"))
    {
      if (strstr(str, "(0)"))
      {
        arr->SetName("x");
        xArray = vtkDataArray::SafeDownCast(arr);
      }
      else
      {
        arr->SetName("y");
        yArray = vtkDataArray::SafeDownCast(arr);
      }
    }
  }

  double bounds[4] = { VTK_DOUBLE_MAX, VTK_DOUBLE_MIN, VTK_DOUBLE_MAX, VTK_DOUBLE_MIN };
  xArray->GetRange(&bounds[0], 0);
  yArray->GetRange(&bounds[2], 0);

  double sigma = this->KernelWidth;
  if (this->UseSilvermanRule)
  {
    vtkIdType len = xArray->GetNumberOfTuples();
    double xMean = 0.0;
    for (vtkIdType i = 0; i < len; i++)
    {
      xMean += xArray->GetTuple1(i);
    }
    xMean /= len;

    sigma = 0.0;
    for (vtkIdType i = 0; i < len; i++)
    {
      sigma += (xArray->GetTuple1(i) - xMean) * (xArray->GetTuple1(i) - xMean);
    }
    sigma /= len;
    sigma = sqrt(sigma) * pow(len, -1. / 6.);
  }

  hdr->SetSigma(sigma);
  hdr->AddColumnPair("x", "y");
  hdr->SetLearnOption(true);
  hdr->SetDeriveOption(true);
  hdr->SetAssessOption(false);
  hdr->SetTestOption(false);
  hdr->Update();

  // Compute Grid
  vtkNew<vtkDoubleArray> inObs;
  inObs->SetNumberOfComponents(2);
  inObs->SetNumberOfTuples(xArray->GetNumberOfTuples());

  inObs->CopyComponent(0, xArray, 0);
  inObs->CopyComponent(1, yArray, 0);

  // Add border to grid
  const double borderSize = 0.15;
  bounds[0] -= (bounds[1] - bounds[0]) * borderSize;
  bounds[1] += (bounds[1] - bounds[0]) * borderSize;
  bounds[2] -= (bounds[3] - bounds[2]) * borderSize;
  bounds[3] += (bounds[3] - bounds[2]) * borderSize;

  const int gridWidth = this->GetGridSize();
  const int gridHeight = this->GetGridSize();
  const double spaceX = (bounds[1] - bounds[0]) / gridWidth;
  const double spaceY = (bounds[3] - bounds[2]) / gridHeight;
  vtkNew<vtkDoubleArray> inPOI;
  inPOI->SetNumberOfComponents(2);
  inPOI->SetNumberOfTuples(gridWidth * gridHeight);

  vtkIdType pointId = 0;
  for (int j = 0; j < gridHeight; j++)
  {
    for (int i = 0; i < gridWidth; i++)
    {
      double x = bounds[0] + i * spaceX;
      double y = bounds[2] + j * spaceY;

      inPOI->SetTuple2(pointId, x, y);
      ++pointId;
    }
  }

  vtkDataArray* outDens = vtkDataArray::CreateDataArray(inObs->GetDataType());
  outDens->SetNumberOfComponents(1);
  outDens->SetNumberOfTuples(gridWidth * gridHeight);

  // Evaluate the HDR on every pixel of the grid
  hdr->ComputeHDR(inObs.Get(), inPOI.Get(), outDens);

  vtkNew<vtkImageData> grid;
  grid->SetDimensions(gridWidth, gridHeight, 1);
  grid->SetOrigin(bounds[0], bounds[2], 0.);
  grid->SetSpacing(spaceX, spaceY, 1.);
  grid->GetPointData()->SetScalars(outDens);

  outDens->Delete();

  // Compute the integral of the density and save the position of the
  // highest density in the grid for further evaluation of the mean.
  vtkDataArray* densities = outDens;
  double totalSumOfDensities = 0.;
  std::vector<double> sortedDensities;
  sortedDensities.reserve(densities->GetNumberOfTuples());
  // double maxDensity = VTK_DOUBLE_MIN;
  // int maxX, maxY;
  for (vtkIdType pixel = 0; pixel < densities->GetNumberOfTuples(); ++pixel)
  {
    double density = densities->GetTuple1(pixel);
    sortedDensities.push_back(density);
    totalSumOfDensities += density;
    /*if (density > maxDensity)
      {
      maxX = pixel % gridWidth;
      maxY = pixel / gridWidth;
      maxDensity = density;
      }*/
  }

  // Sort the densities and save the densities associated to the quantiles.
  std::sort(sortedDensities.begin(), sortedDensities.end());
  double sumOfDensities = 0.;
  double sumForP50 = totalSumOfDensities * 0.5;
  double sumForPUser = totalSumOfDensities * ((100. - this->UserQuantile) / 100.);
  double p50 = 0.;
  double pUser = 0.;
  for (std::vector<double>::const_iterator it = sortedDensities.begin();
       it != sortedDensities.end(); ++it)
  {
    sumOfDensities += *it;
    if (sumOfDensities >= sumForP50 && p50 == 0.)
    {
      p50 = *it;
    }
    if (sumOfDensities >= sumForPUser && pUser == 0.)
    {
      pUser = *it;
    }
  }

  // Save information on the quantiles (% and density) in a specific table.
  // It will be used downstream by the bag plot representation for instance
  // to generate the contours at the provided values.
  vtkNew<vtkTable> thresholdTable;
  vtkNew<vtkDoubleArray> tValues;
  tValues->SetName("TValues");
  tValues->SetNumberOfValues(6);
  tValues->SetValue(0, 50);
  tValues->SetValue(1, p50);
  tValues->SetValue(2, this->UserQuantile);
  tValues->SetValue(3, pUser);
  tValues->SetValue(4, explainedVariance);
  tValues->SetValue(5, sigma);
  thresholdTable->AddColumn(tValues.Get());

  // Bag plot
  vtkMultiBlockDataSet* outputHDR = vtkMultiBlockDataSet::SafeDownCast(
    hdr->GetOutputDataObject(vtkStatisticsAlgorithm::OUTPUT_MODEL));
  vtkTable* outputHDRTable = vtkTable::SafeDownCast(outputHDR->GetBlock(0));
  outTable2 = outputHDRTable;
  vtkAbstractArray* cname = inputTable->GetColumnByName("ColName");
  if (cname)
  {
    outputHDRTable->AddColumn(cname);
  }
  else
  {
    vtkNew<vtkStringArray> colNameArray;
    colNameArray->SetName("ColName");
    vtkIdType len = inputTable->GetNumberOfColumns();
    colNameArray->SetNumberOfValues(len);
    for (vtkIdType i = 0; i < len; i++)
    {
      colNameArray->SetValue(i, inputTable->GetColumn(i)->GetName());
    }
    outputHDRTable->AddColumn(colNameArray.GetPointer());
  }

  // Extract the bag plot columns for functional bag plots
  vtkNew<vtkExtractFunctionalBagPlot> ebp;
  ebp->SetInputData(0, outTable);
  ebp->SetInputData(1, outputHDRTable);
  ebp->SetInputArrayToProcess(0, 1, 0, vtkDataObject::FIELD_ASSOCIATION_ROWS, "HDR (y,x)");
  ebp->SetInputArrayToProcess(1, 1, 0, vtkDataObject::FIELD_ASSOCIATION_ROWS, "ColName");
  ebp->SetDensityForP50(p50);
  ebp->SetDensityForPUser(pUser);
  ebp->SetPUser(this->UserQuantile);
  ebp->Update();

  outTable = ebp->GetOutput();

  double maxHdr = VTK_DOUBLE_MIN;
  std::string maxHdrCName = "";
  vtkDataArray* seriesHdr =
    vtkDataArray::SafeDownCast(outputHDRTable->GetColumnByName("HDR (y,x)"));
  vtkStringArray* seriesColName =
    vtkStringArray::SafeDownCast(outputHDRTable->GetColumnByName("ColName"));
  assert(seriesHdr && seriesColName);
  for (vtkIdType i = 0; i < seriesHdr->GetNumberOfTuples(); i++)
  {
    double v = seriesHdr->GetTuple1(i);
    if (v > maxHdr)
    {
      maxHdr = v;
      maxHdrCName = seriesColName->GetValue(i);
    }
  }

  // Compute the mean function by back-projecting the point of the
  // highest-density with the PCA eigen-vectors and the mean
  vtkDoubleArray* medianFunction =
    vtkDoubleArray::SafeDownCast(outTable->GetColumnByName("QMedianLine"));
  if (medianFunction)
  {
    outTable->RemoveColumnByName(medianFunction->GetName());
  }

  vtkDataArray* maxHdrColumn =
    vtkDataArray::SafeDownCast(outTable->GetColumnByName(maxHdrCName.c_str()));
  assert(maxHdrColumn);
  std::stringstream medianColumnName;
  medianColumnName << maxHdrColumn->GetName() << "_median";
  maxHdrColumn->SetName(medianColumnName.str().c_str());

  /*
  vtkTable* outputMeta =
  vtkTable::SafeDownCast(outputMetaDS->GetBlock(1));
  vtkIdType nbDimensions = eigenVectors->GetNumberOfComponents();
  vtkDoubleArray* meanCol =
    vtkDoubleArray::SafeDownCast(outputMeta->GetColumnByName("Mean"));
  // Compute the median function
  double medianX = bounds[0] + maxX * spaceX;
  double medianY = bounds[2] + maxY * spaceY;
  for (vtkIdType i = 0; i < nbDimensions; i++)
    {
    medianFunction->SetValue(i,
      medianX * eigenVectors->GetComponent(0, i) +
      medianY * eigenVectors->GetComponent(1, i) +
      meanCol->GetValue(i));
    }
  */

  // Finally setup the output multi-block
  unsigned int blockID = 0;
  outTables->SetBlock(blockID, outTable);
  outTables->GetMetaData(blockID)->Set(vtkCompositeDataSet::NAME(), "Functional Bag Plot Data");
  blockID = 1;
  outTables->SetBlock(blockID, outTable2);
  outTables->GetMetaData(blockID)->Set(vtkCompositeDataSet::NAME(), "Bag Plot Data");
  blockID = 2;
  outTables->SetBlock(blockID, grid.Get());
  outTables->GetMetaData(blockID)->Set(vtkCompositeDataSet::NAME(), "Grid Data");
  blockID = 3;
  outTables->SetBlock(blockID, thresholdTable.Get());
  outTables->GetMetaData(blockID)->Set(vtkCompositeDataSet::NAME(), "Threshold Data");

  return 1;
}
