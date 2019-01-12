/*=========================================================================

  Program:   ParaView
  Module:    vtkSpreadSheetView.h

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkSpreadSheetView
 *
 * vtkSpreadSheetView is a vtkPVView subclass for a view used to show any data
 * as a spreadsheet. This view can only show one representation at a
 * time. If more than one representation is added to this view, only the first
 * visible representation will be shown.
*/

#ifndef vtkSpreadSheetView_h
#define vtkSpreadSheetView_h

#include "vtkPVClientServerCoreRenderingModule.h" //needed for exports
#include "vtkPVView.h"

class vtkCSVExporter;
class vtkClientServerMoveData;
class vtkMarkSelectedRows;
class vtkReductionFilter;
class vtkSortedTableStreamer;
class vtkTable;
class vtkVariant;

class VTKPVCLIENTSERVERCORERENDERING_EXPORT vtkSpreadSheetView : public vtkPVView
{
public:
  static vtkSpreadSheetView* New();
  vtkTypeMacro(vtkSpreadSheetView, vtkPVView);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /**
   * Triggers a high-resolution render.
   * \note CallOnAllProcesses
   */
  void StillRender() override { this->StreamToClient(); }

  /**
   * Triggers a interactive render. Based on the settings on the view, this may
   * result in a low-resolution rendering or a simplified geometry rendering.
   * \note CallOnAllProcesses
   */
  void InteractiveRender() override { this->StreamToClient(); }

  /**
   * Overridden to identify and locate the active-representation.
   */
  void Update() override;

  //@{
  /**
   * Get/Set if the view shows extracted selection only or the actual data.
   * false by default.
   * \note CallOnAllProcesses
   */
  void SetShowExtractedSelection(bool);
  vtkBooleanMacro(ShowExtractedSelection, bool);
  vtkGetMacro(ShowExtractedSelection, bool);
  //@}

  //@{
  /**
   * Allow user to enable/disable cell connectivity generation.
   */
  vtkSetMacro(GenerateCellConnectivity, bool);
  vtkGetMacro(GenerateCellConnectivity, bool);
  vtkBooleanMacro(GenerateCellConnectivity, bool);
  //@}

  //@{
  /**
   * Specify the field association for the data to be shown in the view.
   * This is passed on to the vtkSpreadSheetRepresentation in `Update` pass.
   */
  vtkSetMacro(FieldAssociation, int);
  vtkGetMacro(FieldAssociation, int);
  //@}

  //@{
  /**
   * This API enables the users to hide columns that should be shown.
   * Columns can be hidden using their names or labels.
   */
  void HideColumnByName(const char* columnName);
  bool IsColumnHiddenByName(const char* columnName);
  void ClearHiddenColumnsByName();

  void HideColumnByLabel(const char* columnLabel);
  bool IsColumnHiddenByLabel(const char* columnLabel);
  void ClearHiddenColumnsByLabel();
  //@}

  /**
   * Get the number of columns.
   * \note CallOnClient
   */
  vtkIdType GetNumberOfColumns();

  /**
   * Get the number of rows.
   * \note CallOnClient
   */
  vtkIdType GetNumberOfRows();

  /**
   * Returns the name for the column.
   * \note CallOnClient
   */
  const char* GetColumnName(vtkIdType index);

  //@{
  /**
   * Returns true if the column is internal.
   */
  bool IsColumnInternal(vtkIdType index);
  bool IsColumnInternal(const char* columnName);
  //@}

  //@{
  /**
   * Returns the user-friendly label to use for the column
   * in the spreadsheet view.
   *
   * If `this->IsColumnInternal(..)` is true for the chosen column. Then this
   * method will return `nullptr`.
   *
   * \note CallOnClient
   */
  const char* GetColumnLabel(vtkIdType index);
  const char* GetColumnLabel(const char* columnName);
  //@}

  /**
   * Returns the visibility for the column at the given index.
   */
  bool GetColumnVisibility(vtkIdType index);

  //@{
  /**
   * Returns the value at given location. This may result in collective
   * operations is data is not available locally. This method can only be called
   * on the CLIENT process for now.
   * \note CallOnClient
   */
  vtkVariant GetValue(vtkIdType row, vtkIdType col);
  vtkVariant GetValueByName(vtkIdType row, const char* columnName);
  //@}

  /**
   * Returns true if the row is selected.
   */
  bool IsRowSelected(vtkIdType row);

  /**
   * Returns true is the data for the particular row is locally available.
   */
  bool IsAvailable(vtkIdType row);

  //***************************************************************************
  // Forwarded to vtkSortedTableStreamer.
  /**
   * Get/Set the column name to sort by.
   * \note CallOnAllProcesses
   */
  void SetColumnNameToSort(const char*);
  void SetColumnNameToSort() { this->SetColumnNameToSort(NULL); }

  /**
   * Get/Set the component to sort with. Use -1 (default) for magnitude.
   * \note CallOnAllProcesses
   */
  void SetComponentToSort(int val);

  /**
   * Get/Set whether the sort order must be Max to Min rather than Min to Max.
   * \note CallOnAllProcesses
   */
  void SetInvertSortOrder(bool);

  /**
   * Set the block size
   * \note CallOnAllProcesses
   */
  void SetBlockSize(vtkIdType val);

  /**
   * Export the contents of this view using the exporter.
   */
  bool Export(vtkCSVExporter* exporter);

  /**
   * Allow user to clear the cache if he needs to.
   */
  void ClearCache();

  // INTERNAL METHOD. Don't call directly.
  vtkTable* FetchBlockCallback(vtkIdType blockindex);

protected:
  vtkSpreadSheetView();
  ~vtkSpreadSheetView() override;

  /**
   * On render streams all the data from the processes to the client.
   * Returns 0 on failure.
   * Note: Was removed from update because you can't call update()
   * while in an update
   */
  int StreamToClient();

  void OnRepresentationUpdated();

  vtkTable* FetchBlock(vtkIdType blockindex, bool skipCache = false);

  bool ShowExtractedSelection;
  bool GenerateCellConnectivity;
  vtkSortedTableStreamer* TableStreamer;
  vtkMarkSelectedRows* TableSelectionMarker;
  vtkReductionFilter* ReductionFilter;
  vtkClientServerMoveData* DeliveryFilter;
  vtkIdType NumberOfRows;

  enum
  {
    FETCH_BLOCK_TAG = 394732
  };

private:
  vtkSpreadSheetView(const vtkSpreadSheetView&) = delete;
  void operator=(const vtkSpreadSheetView&) = delete;

  class vtkInternals;
  friend class vtkInternals;
  vtkInternals* Internals;
  bool SomethingUpdated;
  unsigned long RMICallbackTag;

  int FieldAssociation;
};

#endif
