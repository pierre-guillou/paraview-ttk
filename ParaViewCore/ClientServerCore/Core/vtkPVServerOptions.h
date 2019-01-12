/*=========================================================================

  Program:   ParaView
  Module:    vtkPVServerOptions.h

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkPVServerOptions
 * @brief   ParaView options for server executables.
 *
 * An object of this class represents a storage for command line options for
 * various server executables.
 *
 * These options can be retrieved during run-time, set using configuration file
 * or using Command Line Arguments.
*/

#ifndef vtkPVServerOptions_h
#define vtkPVServerOptions_h

#include "vtkPVClientServerCoreCoreModule.h" //needed for exports
#include "vtkPVOptions.h"

class vtkPVServerOptionsInternals;

class VTKPVCLIENTSERVERCORECORE_EXPORT vtkPVServerOptions : public vtkPVOptions
{
public:
  static vtkPVServerOptions* New();
  vtkTypeMacro(vtkPVServerOptions, vtkPVOptions);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  //@{
  /**
   * Name of the client-host. By default, the client are server are assumed to
   * be on the same host.
   */
  vtkGetStringMacro(ClientHostName);
  //@}

  //@{
  /**
   * The configuration port for this process. The interpretation of the port
   * number depends on the configuration and process type.
   */
  vtkGetMacro(ServerPort, int);
  //@}

  /**
   * Pass in the name and the attributes for all tags that are not Options.
   * If it returns 1, then it is successful, and 0 if it failed.
   */
  int ParseExtraXMLTag(const char* name, const char** atts) VTK_OVERRIDE;

  //@{
  /**
   * Get information about machines used in a data or render server.
   */
  double GetEyeSeparation();
  unsigned int GetNumberOfMachines();
  const char* GetMachineName(unsigned int idx);
  const char* GetDisplayName(unsigned int idx);
  int* GetGeometry(unsigned int idx);
  bool GetFullScreen(unsigned int idx);
  bool GetShowBorders(unsigned int idx);
  double* GetLowerLeft(unsigned int idx);
  double* GetLowerRight(unsigned int idx);
  double* GetUpperRight(unsigned int idx);
  bool GetCaveBoundsSet(unsigned int idx);
  //@}

  // Returns -1 to indicate not stereo type was specified. 0 indicate no stereo
  // is to be used.
  int GetStereoType(unsigned int idx);
  char* GetStereoType() VTK_OVERRIDE { return this->Superclass::GetStereoType(); }
protected:
  /**
   * Add machine information from the xml tag <Machine ....>
   */
  int AddMachineInformation(const char** atts);

  /**
   * Add eye separation information from the xml tag <EyeSeparation ...>
   */
  int AddEyeSeparationInformation(const char** atts);

  /**
   * Default constructor.
   */
  vtkPVServerOptions();

  /**
   * Destructor.
   */
  ~vtkPVServerOptions() override;

  void Initialize() VTK_OVERRIDE;

  vtkSetStringMacro(ClientHostName);
  char* ClientHostName;

  int ServerPort;

private:
  vtkPVServerOptions(const vtkPVServerOptions&) = delete;
  void operator=(const vtkPVServerOptions&) = delete;

  vtkPVServerOptionsInternals* Internals;
};

#endif // #ifndef vtkPVServerOptions_h
