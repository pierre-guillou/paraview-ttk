/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkPLagrangianParticleTracker.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

    This software is distributed WITHOUT ANY WARRANTY; without even
    the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
    PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkPLagrangianParticleTracker.h"

#include "vtkAppendFilter.h"
#include "vtkBoundingBox.h"
#include "vtkCellData.h"
#include "vtkCompositeDataIterator.h"
#include "vtkGenericCell.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkLagrangianBasicIntegrationModel.h"
#include "vtkLagrangianParticle.h"
#include "vtkLongLongArray.h"
#include "vtkMPIController.h"
#include "vtkMultiBlockDataSet.h"
#include "vtkPointData.h"
#include "vtkPolyData.h"
#include "vtkPolyLine.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkUnstructuredGrid.h"

#include <vector>

#define LAGRANGIAN_PARTICLE_TAG 621
#define LAGRANGIAN_RANG_FLAG_TAG 622
#define LAGRANGIAN_ARRAY_TAG 623

namespace
{
enum CommunicationFlag
{
  VTK_PLAGRANGIAN_WORKING_FLAG = 0,
  VTK_PLAGRANGIAN_EMPTY_FLAG = 1,
  VTK_PLAGRANGIAN_FINISHED_FLAG = 2
};
}

// Class used to serialize and stream a particle
class MessageStream
{
public:
  MessageStream(int BufferSize)
    : Size(BufferSize)
  {
    this->Data.resize(Size);
    this->Head = Data.data();
  }

  ~MessageStream() {}

  int GetSize() { return this->Size; }

  template <class T>
  MessageStream& operator<<(T t)
  {
    size_t size = sizeof(T);
    char* value = reinterpret_cast<char*>(&t);
    for (size_t i = 0; i < size; i++)
    {
      *this->Head++ = *value++;
    }
    return *this;
  }

  template <class T>
  MessageStream& operator>>(T& t)
  {
    size_t size = sizeof(T);
    t = *reinterpret_cast<T*>(this->Head);
    this->Head += size;
    return *this;
  }

  char* GetRawData() { return this->Data.data(); }
  int GetLength() { return this->Head - this->Data.data(); }

  void Reset() { this->Head = this->Data.data(); }

private:
  MessageStream(const MessageStream&) = delete;
  void operator=(const MessageStream&) = delete;

  std::vector<char> Data;
  char* Head;
  int Size;
};

// A singleton class used by each rank to stream particle with another
// It sends particle to all other ranks and can receive particle
// from any other rank.
class ParticleStreamManager
{
public:
  ParticleStreamManager(vtkMPIController* controller, vtkPointData* seedData,
    vtkLagrangianBasicIntegrationModel* model, const vtkBoundingBox* bounds)
  {
    // Initialize Members
    this->Controller = controller;
    this->SeedData = seedData;
    this->WeightsSize = model->GetWeightsSize();

    // Gather bounds and initialize requests
    std::vector<double> allBounds(6 * this->Controller->GetNumberOfProcesses(), 0);
    double nodeBounds[6];
    bounds->GetBounds(nodeBounds);
    this->Controller->AllGather(nodeBounds, &allBounds[0], 6);
    for (int i = 0; i < this->Controller->GetNumberOfProcesses(); i++)
    {
      vtkBoundingBox box;
      box.AddBounds(&allBounds[i * 6]);
      this->Boxes.push_back(box);
    }

    // Compute StreamSize for one particle
    // This is strongly linked to Send and Receive code
    this->StreamSize = sizeof(int) * 2 + sizeof(double) * 2 + 4 * sizeof(vtkIdType) + sizeof(int) +
      2 * sizeof(double) +
      3 * sizeof(double) *
        (model->GetNumberOfIndependentVariables() + model->GetNumberOfTrackedUserData());
    for (int i = 0; i < seedData->GetNumberOfArrays(); i++)
    {
      vtkDataArray* array = seedData->GetArray(i);
      this->StreamSize += array->GetNumberOfComponents() * sizeof(double);
    }

    // Initialize Streams
    this->ReceiveStream = new MessageStream(this->StreamSize);
    this->SendStream = nullptr;
  }

  ~ParticleStreamManager()
  {
    for (size_t i = 0; i < this->SendRequests.size(); i++)
    {
      delete this->SendRequests[i];
    }

    // Delete streams
    delete this->ReceiveStream;
    delete this->SendStream;
  }

  // Method to send a particle to others ranks
  // if particle contained in bounds
  void SendParticle(vtkLagrangianParticle* particle)
  {
    // Serialize particle
    // This is strongly linked to Constructor and Receive code
    delete this->SendStream;
    this->SendStream = new MessageStream(this->StreamSize);
    *this->SendStream << particle->GetSeedId();
    *this->SendStream << particle->GetId();
    *this->SendStream << particle->GetParentId();
    *this->SendStream << particle->GetNumberOfVariables();
    *this->SendStream << static_cast<int>(particle->GetTrackedUserData().size());
    *this->SendStream << particle->GetNumberOfSteps();
    *this->SendStream << particle->GetIntegrationTime();
    *this->SendStream << particle->GetPrevIntegrationTime();
    *this->SendStream << particle->GetUserFlag();
    *this->SendStream << particle->GetPInsertPreviousPosition();
    *this->SendStream << particle->GetPManualShift();

    double* prev = particle->GetPrevEquationVariables();
    double* curr = particle->GetEquationVariables();
    double* next = particle->GetNextEquationVariables();
    for (int i = 0; i < particle->GetNumberOfVariables(); i++)
    {
      *this->SendStream << prev[i];
      *this->SendStream << curr[i];
      *this->SendStream << next[i];
    }

    for (auto data : particle->GetPrevTrackedUserData())
    {
      *this->SendStream << data;
    }
    for (auto data : particle->GetTrackedUserData())
    {
      *this->SendStream << data;
    }
    for (auto data : particle->GetNextTrackedUserData())
    {
      *this->SendStream << data;
    }

    for (int i = 0; i < particle->GetSeedData()->GetNumberOfArrays(); i++)
    {
      vtkDataArray* array = particle->GetSeedData()->GetArray(i);
      double* tuple = array->GetTuple(0);
      for (int j = 0; j < array->GetNumberOfComponents(); j++)
      {
        *this->SendStream << tuple[j];
      }
    }

    // Send to other ranks
    for (int i = 0; i < this->Controller->GetNumberOfProcesses(); i++)
    {
      if (i == this->Controller->GetLocalProcessId())
      {
        continue;
      }
      if (particle->GetPManualShift() || this->Boxes[i].ContainsPoint(particle->GetPosition()))
      {
        this->CleanSendRequests();
        this->SendRequests.push_back(new vtkMPICommunicator::Request);
        this->Controller->NoBlockSend(this->SendStream->GetRawData(), this->StreamSize, i,
          LAGRANGIAN_PARTICLE_TAG, *this->SendRequests.back());
      }
    }
  }

  // Method to receive and deserialize a particle from any other rank
  bool ReceiveParticleIfAny(vtkLagrangianParticle*& particle)
  {
    int probe, source;
    if (this->Controller->Iprobe(
          vtkMultiProcessController::ANY_SOURCE, LAGRANGIAN_PARTICLE_TAG, &probe, &source) &&
      probe)
    {
      this->ReceiveStream->Reset();
      this->Controller->Receive(this->ReceiveStream->GetRawData(), this->StreamSize,
        vtkMultiProcessController::ANY_SOURCE, LAGRANGIAN_PARTICLE_TAG);
      // Deserialize particle
      // This is strongly linked to Constructor and Send method
      int nVar, userFlag, nTrackedUserData;
      vtkIdType seedId, particleId, parentId, nSteps;
      double iTime, prevITime;
      bool pInsertPrevious, pManualShift;
      *this->ReceiveStream >> seedId;
      *this->ReceiveStream >> particleId;
      *this->ReceiveStream >> parentId;
      *this->ReceiveStream >> nVar;
      *this->ReceiveStream >> nTrackedUserData;
      *this->ReceiveStream >> nSteps;
      *this->ReceiveStream >> iTime;
      *this->ReceiveStream >> prevITime;
      *this->ReceiveStream >> userFlag;
      *this->ReceiveStream >> pInsertPrevious;
      *this->ReceiveStream >> pManualShift;

      // Get a particle with an incorrect seed data
      particle = vtkLagrangianParticle::NewInstance(nVar, seedId, particleId, -1, iTime,
        this->SeedData, this->WeightsSize, nTrackedUserData, nSteps, prevITime);
      particle->SetParentId(parentId);
      particle->SetUserFlag(userFlag);
      particle->SetPInsertPreviousPosition(pInsertPrevious);
      particle->SetPManualShift(pManualShift);
      double* prev = particle->GetPrevEquationVariables();
      double* curr = particle->GetEquationVariables();
      double* next = particle->GetNextEquationVariables();
      for (int i = 0; i < nVar; i++)
      {
        *this->ReceiveStream >> prev[i];
        *this->ReceiveStream >> curr[i];
        *this->ReceiveStream >> next[i];
      }

      std::vector<double>& prevTracked = particle->GetPrevTrackedUserData();
      for (auto& var : prevTracked)
      {
        *this->ReceiveStream >> var;
      }
      std::vector<double>& tracked = particle->GetTrackedUserData();
      for (auto& var : tracked)
      {
        *this->ReceiveStream >> var;
      }
      std::vector<double>& nextTracked = particle->GetNextTrackedUserData();
      for (auto& var : nextTracked)
      {
        *this->ReceiveStream >> var;
      }

      // Recover the correct seed data values
      for (int i = 0; i < particle->GetSeedData()->GetNumberOfArrays(); i++)
      {
        vtkDataArray* array = particle->GetSeedData()->GetArray(i);
        int numComponents = array->GetNumberOfComponents();
        std::vector<double> xi(numComponents);
        for (int j = 0; j < numComponents; j++)
        {
          *this->ReceiveStream >> xi[j];
        }
        array->InsertNextTuple(&xi[0]);
      }
      return true;
    }
    return false;
  }

  void CleanSendRequests()
  {
    std::vector<vtkMPICommunicator::Request*>::iterator it = SendRequests.begin();
    while (it != SendRequests.end())
    {
      if ((*it)->Test())
      {
        delete *it;
        it = SendRequests.erase(it);
      }
      else
      {
        ++it;
      }
    }
  }

private:
  vtkMPIController* Controller;
  int StreamSize;
  int WeightsSize;
  MessageStream* SendStream;
  MessageStream* ReceiveStream;
  vtkPointData* SeedData;
  ParticleStreamManager(const ParticleStreamManager&) {}
  std::vector<vtkBoundingBox> Boxes;
  std::vector<vtkMPICommunicator::Request*> SendRequests;
};

// Class used by the master rank to receive and send flag
// to other ranks
class MasterFlagManager
{
public:
  MasterFlagManager(vtkMPIController* controller)
  {
    this->Controller = controller;

    this->NRank = this->Controller->GetNumberOfProcesses() - 1;
    this->RankStates.resize(this->NRank);
    this->SentFlag = nullptr;
    this->SendRequests.resize(this->NRank);
    for (int i = 0; i < this->NRank; i++)
    {
      this->RankStates[i] = VTK_PLAGRANGIAN_WORKING_FLAG;
      this->SendRequests[i] = nullptr;
    }
  }

  ~MasterFlagManager()
  {
    for (int i = 0; i < this->NRank; i++)
    {
      delete this->SendRequests[i];
    }
    delete this->SentFlag;
  }

  // Send a flag to all other ranks
  void SendFlag(int flag)
  {
    delete this->SentFlag;
    this->SentFlag = new int;
    *this->SentFlag = flag;
    for (int i = 0; i < this->NRank; i++)
    {
      if (this->SendRequests[i])
      {
        this->SendRequests[i]->Wait();
        delete this->SendRequests[i];
      }
      this->SendRequests[i] = new vtkMPICommunicator::Request;
      this->Controller->NoBlockSend(
        this->SentFlag, 1, i + 1, LAGRANGIAN_RANG_FLAG_TAG, *this->SendRequests[i]);
    }
  }

  // Receive flag from other ranks
  // This method should not be used directly
  int* UpdateAndGetFlags()
  {
    int probe, source;
    while (this->Controller->Iprobe(
             vtkMultiProcessController::ANY_SOURCE, LAGRANGIAN_RANG_FLAG_TAG, &probe, &source) &&
      probe)
    {
      this->Controller->Receive(&this->RankStates[source - 1], 1, source, LAGRANGIAN_RANG_FLAG_TAG);
    }
    return this->RankStates.data();
  }

  // Return true if all other ranks have the argument flag,
  // false otherwise
  bool LookForSameFlags(int flag)
  {
    this->UpdateAndGetFlags();
    for (int i = 0; i < this->NRank; i++)
    {
      if (this->RankStates[i] != flag)
      {
        return false;
      }
    }
    return true;
  }

  // Return true if any of the other rank have the argument flag
  // false otherwise
  bool LookForAnyFlag(int flag)
  {
    this->UpdateAndGetFlags();
    for (int i = 0; i < this->NRank; i++)
    {
      if (this->RankStates[i] == flag)
      {
        return true;
      }
    }
    return false;
  }

private:
  vtkMPIController* Controller;
  int NRank;
  int* SentFlag;
  std::vector<int> RankStates;
  std::vector<vtkMPICommunicator::Request*> SendRequests;
};

// Class used by non master ranks to communicate with master rank
class RankFlagManager
{
public:
  RankFlagManager(vtkMPIController* controller)
  {
    this->Controller = controller;

    // Initialize flags
    this->LastFlag = VTK_PLAGRANGIAN_WORKING_FLAG;
    this->SentFlag = nullptr;
    this->SendRequest = nullptr;
  }

  ~RankFlagManager()
  {
    delete this->SendRequest;
    delete this->SentFlag;
  }

  // Send a flag to master
  void SendFlag(char flag)
  {
    delete this->SentFlag;
    this->SentFlag = new int;
    *this->SentFlag = flag;
    if (this->SendRequest)
    {
      this->SendRequest->Wait();
      delete this->SendRequest;
    }
    this->SendRequest = new vtkMPICommunicator::Request;
    this->Controller->NoBlockSend(
      this->SentFlag, 1, 0, LAGRANGIAN_RANG_FLAG_TAG, *this->SendRequest);
  }

  // Receive flag from master if any and return it
  int UpdateAndGetFlag()
  {
    int probe;
    while (this->Controller->Iprobe(0, LAGRANGIAN_RANG_FLAG_TAG, &probe, nullptr) && probe)
    {
      this->Controller->Receive(&this->LastFlag, 1, 0, LAGRANGIAN_RANG_FLAG_TAG);
    }
    return this->LastFlag;
  }

private:
  vtkMPIController* Controller;
  int* SentFlag;
  int LastFlag;
  vtkMPICommunicator::Request* SendRequest;
};

vtkStandardNewMacro(vtkPLagrangianParticleTracker);

//---------------------------------------------------------------------------
vtkPLagrangianParticleTracker::vtkPLagrangianParticleTracker()
  : Controller(vtkMPIController::SafeDownCast(vtkMultiProcessController::GetGlobalController()))
  , StreamManager(nullptr)
  , MFlagManager(nullptr)
  , RFlagManager(nullptr)
{
  // To get a correct progress update
  if (this->Controller && this->Controller->GetNumberOfProcesses() > 1)
  {
    this->IntegratedParticleCounterIncrement = this->Controller->GetNumberOfProcesses();
  }
}

//---------------------------------------------------------------------------
vtkPLagrangianParticleTracker::~vtkPLagrangianParticleTracker()
{
  delete RFlagManager;
  delete MFlagManager;
  delete StreamManager;
}

//---------------------------------------------------------------------------
int vtkPLagrangianParticleTracker::RequestUpdateExtent(vtkInformation* vtkNotUsed(request),
  vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  vtkInformation* outInfo = outputVector->GetInformationObject(0);
  int piece = outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_PIECE_NUMBER());
  int numPieces = outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_NUMBER_OF_PIECES());
  int ghostLevel = outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_NUMBER_OF_GHOST_LEVELS());

  vtkInformation* info = inputVector[0]->GetInformationObject(0);
  if (info)
  {
    info->Set(vtkStreamingDemandDrivenPipeline::UPDATE_PIECE_NUMBER(), piece);
    info->Set(vtkStreamingDemandDrivenPipeline::UPDATE_NUMBER_OF_PIECES(), numPieces);
    info->Set(vtkStreamingDemandDrivenPipeline::UPDATE_NUMBER_OF_GHOST_LEVELS(), ghostLevel);
  }

  vtkInformation* sourceInfo = inputVector[1]->GetInformationObject(0);
  if (sourceInfo)
  {
    sourceInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_PIECE_NUMBER(), piece);
    sourceInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_NUMBER_OF_PIECES(), numPieces);
    sourceInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_NUMBER_OF_GHOST_LEVELS(), ghostLevel);
  }

  vtkInformation* surfaceInfo = inputVector[2]->GetInformationObject(0);
  if (surfaceInfo)
  {
    surfaceInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_PIECE_NUMBER(), piece);
    surfaceInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_NUMBER_OF_PIECES(), numPieces);
    surfaceInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_NUMBER_OF_GHOST_LEVELS(), ghostLevel);
  }

  return 1;
}

//---------------------------------------------------------------------------
void vtkPLagrangianParticleTracker::GenerateParticles(const vtkBoundingBox* bounds,
  vtkDataSet* seeds, vtkDataArray* initialVelocities, vtkDataArray* initialIntegrationTimes,
  vtkPointData* seedData, int nVar, std::queue<vtkLagrangianParticle*>& particles)
{
  // Generate particle
  if (this->Controller && this->Controller->GetNumberOfProcesses() > 1)
  {
    this->ParticleCounter = this->Controller->GetLocalProcessId();

    // delete potential remaining managers
    delete RFlagManager;
    delete MFlagManager;
    delete StreamManager;

    // Reduce SeedData Arrays
    int nArrays = seedData->GetNumberOfArrays();
    int actualNArrays;
    int rank = this->Controller->GetLocalProcessId();
    int dummyRank = -1;
    int fullArrayRank;

    // Recover maximum number of arrays
    this->Controller->AllReduce(&nArrays, &actualNArrays, 1, vtkCommunicator::MAX_OP);
    if (actualNArrays != nArrays)
    {
      // This rank does not have the maximum number of arrays
      if (nArrays != 0)
      {
        // this rank have an incorrect number of arrays, not supposed to happen
        vtkErrorMacro("Something went wrong with seed data arrays, discarding arrays");
        for (int i = nArrays - 1; i >= 0; i--)
        {
          seedData->RemoveArray(i);
        }
      }

      // Rank without any seeds, does not have access to the structure of
      // seeds pointData.
      // Recover this information from another rank.
      this->Controller->AllReduce(&dummyRank, &fullArrayRank, 1, vtkCommunicator::MAX_OP);
      int source, size;
      char type;
      int probe = false;
      while (!probe)
      {
        // Wait for the arrays metadata to be sent
        this->Controller->Iprobe(
          fullArrayRank, LAGRANGIAN_ARRAY_TAG, &probe, &source, &type, &size);
      }
      MessageStream stream(size);
      // Receive arrays metadata
      this->Controller->Receive(stream.GetRawData(), size, source, LAGRANGIAN_ARRAY_TAG);
      for (int i = 0; i < actualNArrays; i++)
      {
        // Create arrays according to metadata
        int dataType, nComponents, nameLen, compNameLen;
        stream >> dataType;
        vtkDataArray* array = vtkDataArray::CreateDataArray(dataType);
        stream >> nComponents;
        array->SetNumberOfComponents(nComponents);
        stream >> nameLen;
        std::vector<char> name(nameLen + 1, 0);
        for (int l = 0; l < nameLen; l++)
        {
          stream >> name[l];
        }
        array->SetName(&name[0]);
        for (int idComp = 0; idComp < nComponents; idComp++)
        {
          stream >> compNameLen;
          if (compNameLen > 0)
          {
            std::vector<char> compName(compNameLen + 1, 0);
            for (int compLength = 0; compLength < compNameLen; compLength++)
            {
              stream >> compName[compLength];
            }
            array->SetComponentName(idComp, &compName[0]);
          }
        }
        seedData->AddArray(array);
        array->Delete();
      }
    }
    else
    {
      // This rank contains the correct number of arrays
      this->Controller->AllReduce(&rank, &fullArrayRank, 1, vtkCommunicator::MAX_OP);

      // Select the highest rank containing arrays to be the one to be right about arrays metadata
      if (fullArrayRank == rank)
      {
        // Generate arrays metadata
        int streamSize = 0;
        streamSize += nArrays * 3 * sizeof(int);
        // nArrays * (Datatype + nComponents + strlen(name));
        for (int i = 0; i < nArrays; i++)
        {
          vtkDataArray* array = seedData->GetArray(i);
          const char* name = array->GetName();
          streamSize += static_cast<int>(strlen(name)); // name
          int nComp = array->GetNumberOfComponents();
          for (int idComp = 0; idComp < nComp; idComp++)
          {
            streamSize += sizeof(int);
            const char* compName = array->GetComponentName(idComp);
            if (compName)
            {
              streamSize += static_cast<int>(strlen(compName));
            }
          }
        }
        MessageStream stream(streamSize);
        for (int i = 0; i < nArrays; i++)
        {
          vtkDataArray* array = seedData->GetArray(i);
          stream << array->GetDataType();
          stream << array->GetNumberOfComponents();
          const char* name = array->GetName();
          int nameLen = static_cast<int>(strlen(name));
          stream << nameLen;
          for (int l = 0; l < nameLen; l++)
          {
            stream << name[l];
          }
          for (int idComp = 0; idComp < array->GetNumberOfComponents(); idComp++)
          {
            const char* compName = array->GetComponentName(idComp);
            int compNameLen = 0;
            if (compName)
            {
              compNameLen = static_cast<int>(strlen(compName));
              stream << compNameLen;
              for (int compLength = 0; compLength < compNameLen; compLength++)
              {
                stream << compName[compLength];
              }
            }
            else
            {
              stream << compNameLen;
            }
          }
        }

        // Send to arrays metadata to all other ranks
        for (int i = 0; i < this->Controller->GetNumberOfProcesses(); i++)
        {
          if (i == this->Controller->GetLocalProcessId())
          {
            continue;
          }
          this->Controller->Send(stream.GetRawData(), streamSize, i, LAGRANGIAN_ARRAY_TAG);
        }
      }
      else
      {
        // Other ranks containing correct number of arrays, check metadata is correct
        char type;
        int source, size;
        int probe = false;
        while (!probe)
        {
          // Wait for array metadata
          this->Controller->Iprobe(
            fullArrayRank, LAGRANGIAN_ARRAY_TAG, &probe, &source, &type, &size);
        }
        MessageStream stream(size);
        // Receive array metadata
        this->Controller->Receive(stream.GetRawData(), size, source, LAGRANGIAN_ARRAY_TAG);
        // Check data arrays
        for (int i = 0; i < nArrays; i++)
        {
          vtkDataArray* array = seedData->GetArray(i);
          int dataType, nComponents, nameLen, compNameLen;
          stream >> dataType;
          if (dataType != array->GetDataType())
          {
            vtkErrorMacro("Incoherent dataType between nodes, results may be invalid");
          }
          stream >> nComponents;
          if (nComponents != array->GetNumberOfComponents())
          {
            vtkErrorMacro("Incoherent number of components between nodes, "
                          "results may be invalid");
          }
          const char* localName = array->GetName();
          stream >> nameLen;
          std::vector<char> name(nameLen + 1, 0);
          for (int l = 0; l < nameLen; l++)
          {
            stream >> name[l];
          }
          if (strcmp(&name[0], localName) != 0)
          {
            vtkErrorMacro("Incoherent array names between nodes, "
                          "results may be invalid");
          }
          for (int idComp = 0; idComp < nComponents; idComp++)
          {
            stream >> compNameLen;
            const char* localCompName = array->GetComponentName(idComp);
            std::vector<char> compName(compNameLen + 1, 0);
            for (int compLength = 0; compLength < compNameLen; compLength++)
            {
              stream >> compName[compLength];
            }
            if (localCompName && strcmp(&compName[0], localCompName) != 0)
            {
              vtkErrorMacro("Incoherent array component names between nodes, "
                            "results may be invalid");
            }
          }
        }
      }
    }

    // Create managers
    this->StreamManager =
      new ParticleStreamManager(this->Controller, seedData, this->IntegrationModel, bounds);
    if (this->Controller->GetLocalProcessId() == 0)
    {
      this->MFlagManager = new MasterFlagManager(this->Controller);
    }
    else
    {
      this->RFlagManager = new RankFlagManager(this->Controller);
    }

    // Create and set a dummy particle so FindInLocators can use caching.
    vtkNew<vtkGenericCell> dummyCell;
    vtkLagrangianParticle dummyParticle(
      0, 0, 0, 0, 0, nullptr, this->IntegrationModel->GetWeightsSize(), 0);
    dummyParticle.SetThreadedGenericCell(dummyCell);

    // Generate particle and distribute the ones not in domain to other nodes
    for (vtkIdType i = 0; i < seeds->GetNumberOfPoints(); i++)
    {
      double position[3];
      seeds->GetPoint(i, position);
      double initialIntegrationTime =
        initialIntegrationTimes ? initialIntegrationTimes->GetTuple1(i) : 0;
      vtkIdType particleId = this->GetNewParticleId();
      vtkLagrangianParticle* particle = new vtkLagrangianParticle(nVar, particleId, particleId, i,
        initialIntegrationTime, seedData, this->IntegrationModel->GetWeightsSize(),
        this->IntegrationModel->GetNumberOfTrackedUserData());
      memcpy(particle->GetPosition(), position, 3 * sizeof(double));
      initialVelocities->GetTuple(i, particle->GetVelocity());
      this->IntegrationModel->InitializeParticle(particle);
      if (this->IntegrationModel->FindInLocators(particle->GetPosition(), &dummyParticle))
      {
        particles.push(particle);
      }
      else
      {
        this->StreamManager->SendParticle(particle);
        delete particle;
      }
    }
    this->Controller->Barrier();
    this->ReceiveParticles(particles);
  }
  else
  {
    this->Superclass::GenerateParticles(
      bounds, seeds, initialVelocities, initialIntegrationTimes, seedData, nVar, particles);
  }
}

//---------------------------------------------------------------------------
void vtkPLagrangianParticleTracker::GetParticleFeed(
  std::queue<vtkLagrangianParticle*>& particleQueue)
{
  if (!this->Controller || this->Controller->GetNumberOfProcesses() <= 1)
  {
    return;
  }

  // Receive particles first
  this->ReceiveParticles(particleQueue);

  // Particle queue is still empty
  if (particleQueue.empty())
  {
    if (this->Controller->GetLocalProcessId() == 0)
    {
      bool finished = false;
      do
      {
        // We are master, with no more particle, wait for all ranks to be empty
        if (this->MFlagManager->LookForSameFlags(VTK_PLAGRANGIAN_EMPTY_FLAG))
        {
          // check for new particle
          this->ReceiveParticles(particleQueue);

          // Still empty
          if (particleQueue.empty())
          {
            // Everybody empty now, inform ranks
            this->MFlagManager->SendFlag(VTK_PLAGRANGIAN_EMPTY_FLAG);
            finished = false;
            bool working = false;
            while (!finished && !working)
            {
              // Wait for rank to answer finished or working
              working = this->MFlagManager->LookForAnyFlag(VTK_PLAGRANGIAN_WORKING_FLAG);
              finished = this->MFlagManager->LookForSameFlags(VTK_PLAGRANGIAN_FINISHED_FLAG);
              if (working)
              {
                // A rank received a particle in the meantime and is working,
                // resume the wait
                this->MFlagManager->SendFlag(VTK_PLAGRANGIAN_WORKING_FLAG);
              }
              if (finished)
              {
                // Nobody is working anymore, send finished flag and finish ourself
                this->MFlagManager->SendFlag(VTK_PLAGRANGIAN_FINISHED_FLAG);
              }
            }
          }
        }
        // Receive Particles before looking at flags
        this->ReceiveParticles(particleQueue);
      } while (particleQueue.empty() && !finished);
    }
    else
    {
      // We are a rank with no more particle, send empty flag
      this->RFlagManager->SendFlag(VTK_PLAGRANGIAN_EMPTY_FLAG);
      bool finished = false;
      do
      {
        // Wait for master inform us everybody is empty
        bool allEmpty = (this->RFlagManager->UpdateAndGetFlag() == VTK_PLAGRANGIAN_EMPTY_FLAG);

        // Char for new particles
        this->ReceiveParticles(particleQueue);
        if (!particleQueue.empty())
        {
          // Received a particle, keep on working
          this->RFlagManager->SendFlag(VTK_PLAGRANGIAN_WORKING_FLAG);
        }
        else if (allEmpty)
        {
          // Nobody has a particle anymore, send finished flag
          this->RFlagManager->SendFlag(VTK_PLAGRANGIAN_FINISHED_FLAG);
          bool working = false;
          while (!finished && !working)
          {
            // Wait for master to send finished flag
            int flag = this->RFlagManager->UpdateAndGetFlag();
            if (flag == VTK_PLAGRANGIAN_FINISHED_FLAG)
            {
              // we are finished now
              finished = true;
            }
            else if (flag == VTK_PLAGRANGIAN_WORKING_FLAG)
            {
              // Another rank is working, resume the wait
              this->RFlagManager->SendFlag(VTK_PLAGRANGIAN_EMPTY_FLAG);
              working = true;
            }
          }
        }
      } while (particleQueue.empty() && !finished);
    }
  }
}

//---------------------------------------------------------------------------
int vtkPLagrangianParticleTracker::Integrate(vtkInitialValueProblemSolver* integrator,
  vtkLagrangianParticle* particle, std::queue<vtkLagrangianParticle*>& particleQueue,
  vtkPolyData* particlePathsOutput, vtkPolyLine* particlePath, vtkDataObject* interactionOutput)
{
  if (this->Controller && this->Controller->GetNumberOfProcesses() > 1)
  {
    if (this->GenerateParticlePathsOutput && particle->GetPInsertPreviousPosition())
    {
      // This is a particle from another rank, store a duplicated previous point
      this->InsertPathOutputPoint(particle, particlePathsOutput, particlePath->GetPointIds(), true);
      particle->SetPInsertPreviousPosition(false);
    }
  }

  int ret = this->vtkLagrangianParticleTracker::Integrate(
    integrator, particle, particleQueue, particlePathsOutput, particlePath, interactionOutput);

  if (this->Controller && this->Controller->GetNumberOfProcesses() > 1)
  {
    if (particle->GetTermination() == vtkLagrangianParticle::PARTICLE_TERMINATION_OUT_OF_DOMAIN)
    {
      if (!particle->GetPManualShift())
      {
        particle->SetPInsertPreviousPosition(true);
      }

      // Stream out of domain particles
      std::lock_guard<std::mutex> guard(this->StreamManagerMutex);
      this->StreamManager->SendParticle(particle);
    }
  }
  return ret;
}

//---------------------------------------------------------------------------
void vtkPLagrangianParticleTracker::ReceiveParticles(
  std::queue<vtkLagrangianParticle*>& particleQueue)
{
  vtkLagrangianParticle* receivedParticle;

  // Create and set a dummy particle so FindInLocators can use caching.
  vtkNew<vtkGenericCell> dummyCell;
  vtkLagrangianParticle dummyParticle(
    0, 0, 0, 0, 0, nullptr, this->IntegrationModel->GetWeightsSize(), 0);
  dummyParticle.SetThreadedGenericCell(dummyCell);

  while (this->StreamManager->ReceiveParticleIfAny(receivedParticle))
  {
    // Check for manual shift
    if (receivedParticle->GetPManualShift())
    {
      this->IntegrationModel->ParallelManualShift(receivedParticle);
      receivedParticle->SetPManualShift(false);
    }
    // Receive all particles
    if (this->IntegrationModel->FindInLocators(receivedParticle->GetPosition(), &dummyParticle))
    {
      particleQueue.push(receivedParticle);
    }
    else
    {
      delete receivedParticle;
    }
  }
}

//---------------------------------------------------------------------------
bool vtkPLagrangianParticleTracker::FinalizeOutputs(
  vtkPolyData* particlePathsOutput, vtkDataObject* interactionOutput)
{
  if (this->GenerateParticlePathsOutput && this->Controller &&
    this->Controller->GetNumberOfProcesses() > 1)
  {
    // Construct array with all non outofdomains ids and terminations
    vtkNew<vtkLongLongArray> idTermination;
    vtkNew<vtkLongLongArray> allIdTermination;
    idTermination->Allocate(particlePathsOutput->GetNumberOfCells());
    idTermination->SetNumberOfComponents(2);
    vtkIntArray* terminations =
      vtkIntArray::SafeDownCast(particlePathsOutput->GetCellData()->GetArray("Termination"));
    vtkLongLongArray* ids =
      vtkLongLongArray::SafeDownCast(particlePathsOutput->GetCellData()->GetArray("Id"));
    for (int i = 0; i < particlePathsOutput->GetNumberOfCells(); i++)
    {
      if (terminations->GetValue(i) != vtkLagrangianParticle::PARTICLE_TERMINATION_OUT_OF_DOMAIN)
      {
        idTermination->InsertNextTuple2(ids->GetValue(i), terminations->GetValue(i));
      }
    }
    idTermination->Squeeze();

    // AllGather it
    this->Controller->AllGatherV(idTermination, allIdTermination);

    // Modify current terminations
    for (vtkIdType i = 0; i < allIdTermination->GetNumberOfTuples(); i++)
    {
      vtkIdType id = allIdTermination->GetTuple2(i)[0];
      for (vtkIdType j = 0; j < particlePathsOutput->GetNumberOfCells(); j++)
      {
        if (ids->GetValue(j) == id)
        {
          terminations->SetTuple1(j, allIdTermination->GetTuple2(i)[1]);
        }
      }
    }
  }
  return this->Superclass::FinalizeOutputs(particlePathsOutput, interactionOutput);
}

//---------------------------------------------------------------------------
bool vtkPLagrangianParticleTracker::UpdateSurfaceCacheIfNeeded(vtkDataObject*& surfaces)
{
  if (this->Controller && this->Controller->GetNumberOfProcesses() > 1)
  {
    // Update local cache and reduce cache status
    int localCacheUpdated = this->Superclass::UpdateSurfaceCacheIfNeeded(surfaces);
    int maxLocalCacheUpdated;
    this->Controller->AllReduce(
      &localCacheUpdated, &maxLocalCacheUpdated, 1, vtkCommunicator::MAX_OP);

    if (!maxLocalCacheUpdated)
    {
      // Cache is still valid, use already reduced surface
      if (vtkDataSet::SafeDownCast(surfaces))
      {
        surfaces = this->TmpSurfaceInput;
      }
      else // if (vtkCompositeDataSet::SafeDownCast(surfaces))
      {
        surfaces = this->TmpSurfaceInputMB;
      }
      return false;
    }

    // Local cache has been updated, update temporary reduced surface
    // In Parallel, reduce surfaces on rank 0, which then broadcast them to all ranks.

    // Recover all surfaces on rank 0
    std::vector<vtkSmartPointer<vtkDataObject> > allSurfaces;
    this->Controller->Gather(surfaces, allSurfaces, 0);

    // Manager dataset case
    if (vtkDataSet::SafeDownCast(surfaces))
    {
      if (this->Controller->GetLocalProcessId() == 0)
      {
        // Rank 0 append all dataset together
        vtkNew<vtkAppendFilter> append;
        for (int i = 0; i < this->Controller->GetNumberOfProcesses(); i++)
        {
          vtkDataSet* ds = vtkDataSet::SafeDownCast(allSurfaces[i]);
          if (ds)
          {
            append->AddInputData(ds);
          }
        }
        append->Update();
        this->TmpSurfaceInput->ShallowCopy(append->GetOutput());
      }

      // Broadcast resulting UnstructuredGrid
      this->Controller->Broadcast(this->TmpSurfaceInput, 0);
      surfaces = this->TmpSurfaceInput;
    }

    // Composite case
    else if (vtkCompositeDataSet::SafeDownCast(surfaces))
    {
      if (this->Controller->GetLocalProcessId() == 0)
      {
        // Rank 0 reconstruct Composite tree
        vtkCompositeDataSet* mb = vtkCompositeDataSet::SafeDownCast(surfaces);
        this->TmpSurfaceInputMB->ShallowCopy(mb);
        vtkCompositeDataIterator* iter = mb->NewIterator();
        iter->SkipEmptyNodesOff();
        for (iter->InitTraversal(); !iter->IsDoneWithTraversal(); iter->GoToNextItem())
        {
          // Rank 0 append all dataset together
          vtkNew<vtkAppendFilter> append;
          for (int i = 0; i < this->Controller->GetNumberOfProcesses(); i++)
          {
            vtkCompositeDataSet* localMb = vtkCompositeDataSet::SafeDownCast(allSurfaces[i]);
            vtkDataSet* ds = vtkDataSet::SafeDownCast(localMb->GetDataSet(iter));
            if (ds)
            {
              append->AddInputData(ds);
            }
          }
          append->Update();
          this->TmpSurfaceInputMB->SetDataSet(iter, append->GetOutput());
        }
        iter->Delete();
      }
      // Broadcast resulting Composite
      this->Controller->Broadcast(this->TmpSurfaceInputMB, 0);
      surfaces = this->TmpSurfaceInputMB;
    }
    else
    {
      vtkErrorMacro("Unrecognized surface.");
    }
    return true;
  }
  else
  {
    return this->Superclass::UpdateSurfaceCacheIfNeeded(surfaces);
  }
}

//---------------------------------------------------------------------------
vtkIdType vtkPLagrangianParticleTracker::GetNewParticleId()
{
  if (this->Controller && this->Controller->GetNumberOfProcesses() > 1)
  {
    vtkIdType id = this->ParticleCounter;
    this->ParticleCounter += this->Controller->GetNumberOfProcesses();
    return id;
  }
  return this->Superclass::GetNewParticleId();
}

//---------------------------------------------------------------------------
void vtkPLagrangianParticleTracker::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
