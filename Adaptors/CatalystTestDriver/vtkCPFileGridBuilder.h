// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @class   vtkCPFileGridBuilder
 * @brief   Class for creating grids from a VTK file.
 *
 * Class for creating grids from a VTK file.
 */

#ifndef vtkCPFileGridBuilder_h
#define vtkCPFileGridBuilder_h

#include "vtkCPGridBuilder.h"
#include "vtkPVCatalystTestDriverModule.h" // needed for export macros

class vtkDataObject;
class vtkCPFieldBuilder;

class VTKPVCATALYSTTESTDRIVER_EXPORT vtkCPFileGridBuilder : public vtkCPGridBuilder
{
public:
  vtkTypeMacro(vtkCPFileGridBuilder, vtkCPGridBuilder);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /**
   * Return a grid.  BuiltNewGrid is set to 0 if the grids
   * that were returned were already built before.
   * vtkCPFileGridBuilder will also delete the grid.
   */
  vtkDataObject* GetGrid(unsigned long timeStep, double time, int& builtNewGrid) override;

  ///@{
  /**
   * Set/get the FileName.
   */
  vtkGetStringMacro(FileName);
  vtkSetStringMacro(FileName);
  ///@}

  ///@{
  /**
   * Set/get KeepPointData.
   */
  vtkGetMacro(KeepPointData, bool);
  vtkSetMacro(KeepPointData, bool);
  ///@}

  ///@{
  /**
   * Set/get KeepPointData.
   */
  vtkGetMacro(KeepCellData, bool);
  vtkSetMacro(KeepCellData, bool);
  ///@}

  /**
   * Get the current grid.
   */
  vtkDataObject* GetGrid();

protected:
  vtkCPFileGridBuilder();
  ~vtkCPFileGridBuilder() override;

  /**
   * Function to set the grid and take care of the reference counting.
   */
  virtual void SetGrid(vtkDataObject*);

private:
  vtkCPFileGridBuilder(const vtkCPFileGridBuilder&) = delete;

  void operator=(const vtkCPFileGridBuilder&) = delete;

  /**
   * The name of the VTK file to be read.
   */
  char* FileName;

  /**
   * Flag to indicate that any vtkPointData arrays that are set by the
   * file reader are to be cleared out.  By default this is true.
   */
  bool KeepPointData;

  /**
   * Flag to indicate that any vtkCellData arrays that are set by the
   * file reader are to be cleared out.  By default this is true.
   */
  bool KeepCellData;

  ///@{
  /**
   * The grid that is returned.
   */
  vtkDataObject* Grid;
};
//@}

#endif
