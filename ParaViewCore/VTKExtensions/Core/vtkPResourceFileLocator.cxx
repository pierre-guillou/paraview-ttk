/*=========================================================================

  Program:   ParaView
  Module:    vtkPResourceFileLocator.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkPResourceFileLocator.h"

#include "vtkMultiProcessController.h"
#include "vtkMultiProcessStream.h"
#include "vtkObjectFactory.h"

#define VTK_PRESOURCE_FILE_LOCATOR_DEBUG(x)                                                        \
  if (this->PrintDebugInformation)                                                                 \
  {                                                                                                \
    cout << "# pv: " x << endl;                                                                    \
  }

vtkStandardNewMacro(vtkPResourceFileLocator);
//----------------------------------------------------------------------------
vtkPResourceFileLocator::vtkPResourceFileLocator()
{
}

//----------------------------------------------------------------------------
vtkPResourceFileLocator::~vtkPResourceFileLocator()
{
}

//----------------------------------------------------------------------------
std::string vtkPResourceFileLocator::Locate(const std::string& anchor,
  const std::vector<std::string>& landmark_prefixes, const std::string& landmark,
  const std::string& defaultDir)
{
  vtkMultiProcessController* controller = vtkMultiProcessController::GetGlobalController();

  std::string result = (controller == nullptr || controller->GetLocalProcessId() == 0)
    ? this->Superclass::Locate(anchor, landmark_prefixes, landmark, defaultDir)
    : std::string();

  if (controller != nullptr && controller->GetNumberOfProcesses() > 1)
  {
    const int myRank = controller->GetLocalProcessId();
    vtkMultiProcessStream stream;
    if (myRank == 0)
    {
      stream << result;
    }
    if (controller->Broadcast(stream, 0) && myRank > 0)
    {
      stream >> result;
      VTK_PRESOURCE_FILE_LOCATOR_DEBUG("received from rank 0: '" << result << "'");
    }
  }
  return result;
}

//----------------------------------------------------------------------------
void vtkPResourceFileLocator::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
