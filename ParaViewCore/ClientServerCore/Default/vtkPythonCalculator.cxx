/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkPythonCalculator.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkPythonCalculator.h"

#include "vtkCellData.h"
#include "vtkDataArray.h"
#include "vtkDataObject.h"
#include "vtkDataObjectTypes.h"
#include "vtkDataSet.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkPVOptions.h"
#include "vtkPointData.h"
#include "vtkProcessModule.h"
#include "vtkPythonInterpreter.h"

#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <vtksys/SystemTools.hxx>

vtkStandardNewMacro(vtkPythonCalculator);

//----------------------------------------------------------------------------
vtkPythonCalculator::vtkPythonCalculator()
{
  this->Expression = NULL;
  this->ArrayName = NULL;
  this->SetArrayName("result");
  this->SetExecuteMethod(vtkPythonCalculator::ExecuteScript, this);
  this->ArrayAssociation = vtkDataObject::FIELD_ASSOCIATION_POINTS;
}

//----------------------------------------------------------------------------
vtkPythonCalculator::~vtkPythonCalculator()
{
  this->SetExpression(NULL);
  this->SetArrayName(NULL);
}

//----------------------------------------------------------------------------
int vtkPythonCalculator::RequestDataObject(vtkInformation* vtkNotUsed(request),
  vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  // Output type is same as input
  vtkInformation* inInfo = inputVector[0]->GetInformationObject(0);
  if (!inInfo)
  {
    return 0;
  }
  vtkDataObject* input = inInfo->Get(vtkDataObject::DATA_OBJECT());
  if (input)
  {
    // for each output
    for (int i = 0; i < this->GetNumberOfOutputPorts(); ++i)
    {
      vtkInformation* info = outputVector->GetInformationObject(i);
      vtkDataObject* output = info->Get(vtkDataObject::DATA_OBJECT());

      if (!output || !output->IsA(input->GetClassName()))
      {
        vtkDataObject* newOutput = input->NewInstance();
        info->Set(vtkDataObject::DATA_OBJECT(), newOutput);
        newOutput->Delete();
        this->GetOutputPortInformation(0)->Set(
          vtkDataObject::DATA_EXTENT_TYPE(), newOutput->GetExtentType());
      }
    }
    return 1;
  }
  return 0;
}

//----------------------------------------------------------------------------
void vtkPythonCalculator::ExecuteScript(void* arg)
{
  vtkPythonCalculator* self = static_cast<vtkPythonCalculator*>(arg);
  if (self)
  {
    self->Exec(self->GetExpression());
  }
}

//----------------------------------------------------------------------------
void vtkPythonCalculator::Exec(const char* expression)
{
  // Do not execute if expression is null or empty.
  if (!expression || expression[0] == '\0')
  {
    return;
  }

  // Replace tabs with two spaces
  std::string orgscript;
  size_t len = strlen(expression);
  for (size_t i = 0; i < len; i++)
  {
    if (expression[i] == '\t')
    {
      orgscript += "  ";
    }
    else
    {
      orgscript.push_back(expression[i]);
    }
  }
  std::replace(orgscript.begin(), orgscript.end(), '\'', '"');

  // Set self to point to this
  char addrofthis[1024];
  sprintf(addrofthis, "%p", this);
  char* aplus = addrofthis;
  if ((addrofthis[0] == '0') && ((addrofthis[1] == 'x') || addrofthis[1] == 'X'))
  {
    aplus += 2; // skip over "0x"
  }

  std::ostringstream python_stream;
  python_stream << "import paraview\n"
                << "from paraview import calculator\n"
                << "from paraview.vtk.vtkPVClientServerCoreDefault import vtkPythonCalculator\n"
                << "calculator.execute(vtkPythonCalculator('" << aplus << "'), '"
                << orgscript.c_str() << "')\n";

  vtkPythonInterpreter::Initialize();
  vtkPythonInterpreter::RunSimpleString(python_stream.str().c_str());
}

//----------------------------------------------------------------------------
int vtkPythonCalculator::FillOutputPortInformation(int vtkNotUsed(port), vtkInformation* info)
{
  // now add our info
  info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkDataObject");
  return 1;
}

//----------------------------------------------------------------------------
int vtkPythonCalculator::FillInputPortInformation(int port, vtkInformation* info)
{
  if (!this->Superclass::FillInputPortInformation(port, info))
  {
    return 0;
  }
  if (port == 0)
  {
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
    info->Append(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkCompositeDataSet");
    info->Set(vtkAlgorithm::INPUT_IS_REPEATABLE(), 1);
    info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);
  }
  return 1;
}

//----------------------------------------------------------------------------
void vtkPythonCalculator::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
