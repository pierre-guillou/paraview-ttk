/*=========================================================================

  Program:   ParaView
  Module:    vtkSMInputFileNameDomain.h

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkSMInputFileNameDomain
 * @brief   a string domain that can be set automatically
 * with the source file name
 *
 * vtkSMInputFileNameDomain does not really restrict the values of the property
 * that contains it. All string values are valid. Rather, it is used to
 * annotate a pipeline with the source name. This domain
 * works with only vtkSMStringVectorProperty.
 * @sa
 * vtkSMDomain vtkSMStringVectorProperty
*/

#ifndef vtkSMInputFileNameDomain_h
#define vtkSMInputFileNameDomain_h

#include "vtkPVServerManagerCoreModule.h" //needed for exports
#include "vtkSMStringListDomain.h"

class VTKPVSERVERMANAGERCORE_EXPORT vtkSMInputFileNameDomain : public vtkSMStringListDomain
{
public:
  static vtkSMInputFileNameDomain* New();
  vtkTypeMacro(vtkSMInputFileNameDomain, vtkSMStringListDomain);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  // Update self checking the "unchecked" values of all required
  // properties. Overwritten by sub-classes.
  void Update(vtkSMProperty*) VTK_OVERRIDE;

  vtkGetMacro(FileName, std::string);

  /**
   * This method is over-written to automatically update the
   * default filename at run time. The default filename
   * depends on the filename of the source.
   * Returns 1 if the domain updated the property.
   */
  int SetDefaultValues(vtkSMProperty*, bool use_unchecked_values) VTK_OVERRIDE;

protected:
  vtkSMInputFileNameDomain();
  ~vtkSMInputFileNameDomain() override;

private:
  vtkSMInputFileNameDomain(const vtkSMInputFileNameDomain&) = delete;
  void operator=(const vtkSMInputFileNameDomain&) = delete;

  std::string FileName;
};

#endif
