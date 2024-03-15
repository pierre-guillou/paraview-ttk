// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

#include "vtkBandFiltering.h"

#include "vtkArrayDispatch.h"
#include "vtkDSPIterator.h"
#include "vtkDataArray.h"
#include "vtkDataArrayRange.h"
#include "vtkDataSetAttributes.h"
#include "vtkDoubleArray.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMath.h"
#include "vtkMultiDimensionalArray.h"
#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkSMPTools.h"
#include "vtkSmartPointer.h"
#include "vtkTable.h"
#include "vtkType.h"

#include <array>
#include <numeric>
#include <string>
#include <vector>

namespace
{
/// Return x clamped in [min;max] range
inline double Clamp(double x, double min, double max)
{
  return std::min(std::max(x, min), max);
}

/// Return the overlapping factor of r1 relative to r2 :
/// - if r2 is totally inside r1 return 1
/// - if r2 is totally outside r1 return 0
/// - else return the percentage of r2 inside r1
double Overlap(std::array<double, 2> r1, std::array<double, 2> r2)
{
  const double cmin = Clamp(r1[0], r2[0], r2[1]);
  const double cmax = Clamp(r1[1], r2[0], r2[1]);
  return (cmax - cmin) / (r2[1] - r2[0]);
}

/// Represent a frequency band. A band is represented by its lower and upper
/// frequency. Because the FFT return bins of FFT and generated bands can
/// overlap 2 bins at the same time, each band limit has a Ratio that represents
/// how much the limit is overlapping with the FFT bin.
struct Band
{
  struct Limit
  {
    std::size_t Index; ///< Index of the frequency bin in the FFT frequency array
    double Ratio;      ///< Overlap ratio of the limit with the FFT bin
  };
  Limit Lower;
  Limit Upper;
};

/// Given the FFT frequency array and the bandwidth, construct the frequency bands
/// limits that will be used for generating the fitlered values per band later.
/// Also construct the xAxis which contains the range for each generated bin so
/// that user can plot the bins nicely.
///
/// See `struct Band`
std::vector<Band> GenerateOctaveBands(
  vtkDataArray* frequencies, double bandWidth, vtkDoubleArray* xAxis)
{
  // Compute frequency range. Always ignore bin of frequency = 0
  double frange[2] = { frequencies->GetTuple1(0),
    frequencies->GetTuple1(frequencies->GetNumberOfTuples() - 1) };
  if (frange[0] == 0.0)
  {
    frange[0] = frequencies->GetTuple1(1);
  }

  // From IEC 6126
  constexpr double F_BASE = 1000.0;
  constexpr double F_RATIO = 1.9952623149688795; // -> pow(10, 0.3);

  const int lowestBand =
    static_cast<int>(std::floor(bandWidth * std::log10(frange[0] / F_BASE) / 0.3 + 0.5)) + 1;
  const int highestBand =
    static_cast<int>(std::floor(bandWidth * std::log10(frange[1] / F_BASE) / 0.3 + 0.5)) + 1;

  const int nBand = highestBand - lowestBand;
  if (nBand <= 0)
  {
    vtkGenericWarningMacro("Cannot create band spectrum of width " << bandWidth << ": too narrow");
    return {};
  }

  // Construct xAxis array as well as the LUT for constructing bands later on
  xAxis->SetName("Frequency");
  xAxis->SetNumberOfComponents(1);
  xAxis->SetNumberOfTuples(nBand * 2);
  std::vector<Band> bands(nBand);
  const double halfBinSize = (frequencies->GetTuple1(1) - frequencies->GetTuple1(0)) / 2;

  vtkSMPTools::For(0, nBand, [&](int begin, int end) {
    for (int i = begin; i < end; ++i)
    {
      const int currentBand = lowestBand + i;
      const std::array<double, 2> bandLimits = { F_BASE *
          std::pow(F_RATIO, (currentBand - 0.5) / bandWidth),
        F_BASE * std::pow(F_RATIO, (currentBand + 0.5) / bandWidth) };

      xAxis->SetValue(i * 2, bandLimits[0]);
      xAxis->SetValue(i * 2 + 1, bandLimits[1]);

      const auto fArrayRange = vtk::DataArrayValueRange<1>(frequencies);

      const auto lowerIndex =
        std::lower_bound(fArrayRange.cbegin(), fArrayRange.cend(), bandLimits[0] - halfBinSize);
      bands[i].Lower.Index = std::distance(fArrayRange.cbegin(), lowerIndex);
      bands[i].Lower.Ratio =
        ::Overlap(bandLimits, { *lowerIndex - halfBinSize, *lowerIndex + halfBinSize });

      const auto upperIndex =
        std::upper_bound(fArrayRange.cbegin(), fArrayRange.cend(), bandLimits[1] + halfBinSize) - 1;
      bands[i].Upper.Index = std::distance(fArrayRange.cbegin(), upperIndex);
      bands[i].Upper.Ratio =
        ::Overlap(bandLimits, { *upperIndex - halfBinSize, *upperIndex + halfBinSize });
    }
  });

  return bands;
}

/// Given the FFT and the frequency bands to generate, returns the averaged value of column
/// per band.
vtkSmartPointer<vtkDataArray> ProcessColumn(
  vtkDataArray* column, const std::vector<::Band>& bands, bool decibel, double reference)
{
  if (column == nullptr || vtksys::SystemTools::Strucmp(column->GetName(), "Frequency") == 0 ||
    column->GetNumberOfTuples() == 0 || column->GetNumberOfComponents() != 2)
  {
    return nullptr;
  }

  std::vector<double> amplitudes;
  amplitudes.resize(column->GetNumberOfTuples());
  auto inputRange = vtk::DataArrayTupleRange<2>(column);
  vtkSMPTools::Transform(inputRange.cbegin(), inputRange.cend(), amplitudes.begin(),
    [decibel, reference](decltype(inputRange)::ConstTupleReferenceType complex) {
      double tuple[2];
      complex->GetTuple(tuple);
      const double mag =
        decibel ? 20.0 * std::log10(vtkMath::Norm2D(tuple) / reference) : vtkMath::Norm2D(tuple);
      return mag;
    });

  auto resultBands = vtkSmartPointer<vtkDoubleArray>::New();
  resultBands->SetNumberOfComponents(1);
  resultBands->SetNumberOfValues(bands.size() * 2);
  resultBands->SetName(column->GetName());

  vtkSMPTools::For(0, bands.size(), [&](std::size_t begin, std::size_t end) {
    for (std::size_t bandIdx = begin; bandIdx < end; ++bandIdx)
    {
      const auto& band = bands[bandIdx];
      double mean = 0.0;
      mean += amplitudes[band.Lower.Index] * band.Lower.Ratio;
      mean += amplitudes[band.Upper.Index] * band.Upper.Ratio;
      double divider = band.Lower.Ratio + band.Upper.Ratio;

      for (std::size_t i = band.Lower.Index + 1; i < band.Upper.Index; ++i)
      {
        mean += amplitudes[i];
        divider += 1.0;
      }

      mean /= divider;
      resultBands->SetValue(bandIdx * 2, mean);
      resultBands->SetValue(bandIdx * 2 + 1, mean);
    }
  });

  return resultBands;
}

// Additional structure to support multi dimensional array in the band filtering filter
struct Aggregator
{
public:
  virtual void operator()(vtkDataArray* array) = 0;
  virtual vtkSmartPointer<vtkDataArray> GetAggregate() = 0;
  virtual ~Aggregator() = default;
};

template <typename ArrayT>
struct TypedAggregator : Aggregator
{
public:
  using ValueTypeT = vtk::GetAPIType<ArrayT>;

  TypedAggregator(ArrayT* input)
  {
    if (input)
    {
      this->Name = input->GetName();
      this->NumberOfComponents =
        (input->GetNumberOfComponents() != 0 ? input->GetNumberOfComponents() : 1);
    }
  }

  void operator()(vtkDataArray* array) override
  {
    if (!array)
    {
      vtkErrorWithObjectMacro(nullptr, "Could not aggregate array");
      return;
    }
    auto typedArray = ArrayT::FastDownCast(array);
    if (!typedArray)
    {
      typedArray = ArrayT::SafeDownCast(array);
    }

    if (!typedArray)
    {
      vtkErrorWithObjectMacro(nullptr, "Could not aggregate array " << array->GetName());
      return;
    }

    auto range = vtk::DataArrayValueRange(typedArray);
    std::vector<ValueTypeT> buffer(range.size());
    vtkSMPTools::Transform(
      range.begin(), range.end(), buffer.begin(), [](ValueTypeT val) { return val; });
    this->Data->emplace_back(std::move(buffer));
  }

  vtkSmartPointer<vtkDataArray> GetAggregate() override
  {
    vtkNew<vtkMultiDimensionalArray<ValueTypeT>> output;
    output->SetName(this->Name.c_str());
    output->ConstructBackend(
      this->Data, this->Data->at(0).size() / this->NumberOfComponents, this->NumberOfComponents);
    return output;
  }

  ~TypedAggregator() override = default;

private:
  std::string Name;
  vtkIdType NumberOfComponents = 1;
  std::shared_ptr<std::vector<std::vector<ValueTypeT>>> Data =
    std::make_shared<std::vector<std::vector<ValueTypeT>>>();
};

struct DispatchInitializeAggregator
{
  template <typename ArrayT>
  void operator()(ArrayT* arr, std::shared_ptr<Aggregator>& aggregator)
  {
    aggregator = std::make_shared<TypedAggregator<ArrayT>>(arr);
  }
};

};

VTK_ABI_NAMESPACE_BEGIN

//------------------------------------------------------------------------------
vtkStandardNewMacro(vtkBandFiltering);

//----------------------------------------------------------------------------
int vtkBandFiltering::RequestData(
  vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  auto inInfo = inputVector[0]->GetInformationObject(0);
  if (!inInfo)
  {
    vtkErrorMacro("Could not retrieve input vector");
    return 0;
  }

  vtkDataObject* input = inInfo->Get(vtkDataObject::DATA_OBJECT());
  if (!input)
  {
    return 1;
  }

  auto dspIterator = vtkDSPIterator::GetInstance(input);

  bool isFirstRun = true;
  std::vector<std::shared_ptr<::Aggregator>> aggregators;
  vtkSmartPointer<vtkInformationVector> filteredInput;
  for (dspIterator->GoToFirstItem(); !dspIterator->IsDoneWithTraversal();
       dspIterator->GoToNextItem())
  {
    filteredInput = vtkSmartPointer<vtkInformationVector>::New();
    filteredInput->Copy(inputVector[0], true); // deep copy
    filteredInput->GetInformationObject(0)->Set(
      vtkDataObject::DATA_OBJECT(), dspIterator->GetCurrentTable());
    vtkInformationVector* filteredVectorInput[1];
    filteredVectorInput[0] = filteredInput.Get();
    if (this->ExecuteBandFilteringOnTable(request, filteredVectorInput, outputVector) == 0)
    {
      vtkErrorMacro("Error executing superclass filter on data");
      return 0;
    }

    auto outInfo = outputVector->GetInformationObject(0);
    if (!outInfo)
    {
      vtkErrorMacro("Could not get output information");
      return 0;
    }

    auto result = vtkTable::SafeDownCast(outInfo->Get(vtkDataObject::DATA_OBJECT()));
    if (!result)
    {
      vtkErrorMacro("Could not get output as a table");
      return 0;
    }

    if (isFirstRun)
    {
      for (vtkIdType iArr = 0; iArr < result->GetRowData()->GetNumberOfArrays(); ++iArr)
      {
        auto arr = result->GetRowData()->GetArray(iArr);
        if (!arr)
        {
          continue;
        }

        using SupportedArrays = vtkArrayDispatch::Arrays;
        using Dispatcher = vtkArrayDispatch::DispatchByArray<SupportedArrays>;

        std::shared_ptr<::Aggregator> aggregator;
        ::DispatchInitializeAggregator init;
        if (!Dispatcher::Execute(arr, init, aggregator))
        {
          init(arr, aggregator);
        }
        aggregators.emplace_back(std::move(aggregator));
      }
      isFirstRun = false;
    }

    vtkIdType iArr = 0;
    for (auto aggregator : aggregators)
    {
      (*aggregator)(result->GetRowData()->GetArray(iArr));
      iArr++;
    }
  }

  vtkNew<vtkTable> output;
  for (auto aggregator : aggregators)
  {
    output->GetRowData()->AddArray(aggregator->GetAggregate());
  }

  auto outInfo = outputVector->GetInformationObject(0);
  if (!outInfo)
  {
    vtkErrorMacro("Could not get output information");
    return 0;
  }
  outInfo->Set(vtkDataObject::DATA_OBJECT(), output);

  return 1;
}

//----------------------------------------------------------------------------
int vtkBandFiltering::ExecuteBandFilteringOnTable(vtkInformation* vtkNotUsed(request),
  vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  vtkSmartPointer<vtkTable> input = vtkTable::GetData(inputVector[0]);
  auto output = vtkTable::GetData(outputVector);
  if (!input || !output)
  {
    vtkErrorMacro("Input/Output is not initialized");
    return 0;
  }
  if (input->GetNumberOfColumns() <= 0)
  {
    return 1;
  }

  // Apply the FFT on the input if necessary and get the frequency bins of the input
  this->UpdateProgress(0.0);
  vtkSmartPointer<vtkDataArray> frequencies;
  if (this->ApplyFFT)
  {
    input = vtkBandFiltering::ApplyFFTInternal(input, this->WindowType, this->DefaultSamplingRate);
    frequencies = vtkDataArray::SafeDownCast(input->GetColumnByName("Frequency"));
  }
  else
  {
    for (vtkIdType i = 0; i < input->GetNumberOfColumns(); ++i)
    {
      vtkDataArray* column = vtkDataArray::SafeDownCast(input->GetColumn(i));
      if (column && this->FrequencyArrayName == column->GetName())
      {
        frequencies = column;
        break;
      }
    }

    if (frequencies.Get() == nullptr)
    {
      // in this case we have to create the range of frequencies ourself
      // We always consider that the input is an FFT with its mirrored
      // part discarded
      auto* dblFrequencies = vtkDoubleArray::New();
      dblFrequencies->SetNumberOfValues(input->GetNumberOfRows());
      auto range = vtk::DataArrayValueRange<1>(dblFrequencies);
      const double sampleSpacing = this->DefaultSamplingRate / (2 * (range.size() - 1));
      for (vtkIdType i = 0; i < range.size(); ++i)
      {
        range[i] = i * sampleSpacing;
      }
      frequencies.TakeReference(dblFrequencies);
    }
  }
  this->UpdateProgress(0.5);

  // Generate LUT for each frequency bands, as well as the new frequency column
  // corresponding to the frequency bounds of each bands
  double bandWidth = static_cast<double>(this->OctaveSubdivision);
  if (this->BandFilteringMode == OCTAVE)
  {
    bandWidth = 1.0;
  }
  else if (this->BandFilteringMode == THIRD_OCTAVE)
  {
    bandWidth = 3.0;
  }
  vtkNew<vtkDoubleArray> xAxis;
  auto bands = ::GenerateOctaveBands(frequencies, bandWidth, xAxis);
  if (bands.empty())
  {
    return 1;
  }
  output->AddColumn(xAxis);

  // Process all compatible arrays
  this->SetProgressShiftScale(0.5, 0.5);
  for (vtkIdType colID = 0; colID < input->GetNumberOfColumns(); colID++)
  {
    auto resultBands = ::ProcessColumn(vtkDataArray::SafeDownCast(input->GetColumn(colID)), bands,
      this->OutputInDecibel, this->ReferenceValue);
    if (resultBands.Get() != nullptr)
    {
      output->AddColumn(resultBands);
    }
    this->UpdateProgress(static_cast<double>(colID) / input->GetNumberOfColumns());
  }
  this->SetProgressShiftScale(0.0, 1.0);

  return 1;
}

//----------------------------------------------------------------------------
vtkSmartPointer<vtkTable> vtkBandFiltering::ApplyFFTInternal(
  vtkTable* input, int window, double defaultSampleRate)
{
  bool couldReturnOnesided = true;
  for (vtkIdType col = 0; col < input->GetNumberOfColumns(); col++)
  {
    if (input->GetColumn(col)->GetNumberOfComponents() == 2)
    {
      couldReturnOnesided = false;
      break;
    }
  }

  vtkNew<vtkTableFFT> tableFFT;
  tableFFT->SetInputData(input);
  tableFFT->SetReturnOnesided(couldReturnOnesided);
  tableFFT->CreateFrequencyColumnOn();
  tableFFT->SetWindowingFunction(window);
  tableFFT->SetDefaultSampleRate(defaultSampleRate);
  tableFFT->Update();
  vtkSmartPointer<vtkTable> processTable = tableFFT->GetOutput();

  // Drop the second half of the table if we couldn't optimize the FFT :
  // we only process the real part of the FFT
  if (!couldReturnOnesided)
  {
    processTable->SetNumberOfRows(processTable->GetNumberOfRows() / 2 + 1);
  }

  return processTable;
}

//----------------------------------------------------------------------------
void vtkBandFiltering::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  os << indent << "ApplyFFT: " << this->ApplyFFT << std::endl;
  os << indent << "DefaultSamplingRate: " << this->DefaultSamplingRate << std::endl;
  os << indent << "WindowType: " << this->WindowType << std::endl;
  os << indent << "BandFilteringMode: " << this->BandFilteringMode << std::endl;
  os << indent << "OutputInDecibel: " << this->OutputInDecibel << std::endl;
}
