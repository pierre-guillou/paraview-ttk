/*****************************************************************************
*
* Copyright (c) 2000 - 2017, Lawrence Livermore National Security, LLC
* Produced at the Lawrence Livermore National Laboratory
* LLNL-CODE-442911
* All rights reserved.
*
* This file is  part of VisIt. For  details, see https://visit.llnl.gov/.  The
* full copyright notice is contained in the file COPYRIGHT located at the root
* of the VisIt distribution or at http://www.llnl.gov/visit/copyright.html.
*
* Redistribution  and  use  in  source  and  binary  forms,  with  or  without
* modification, are permitted provided that the following conditions are met:
*
*  - Redistributions of  source code must  retain the above  copyright notice,
*    this list of conditions and the disclaimer below.
*  - Redistributions in binary form must reproduce the above copyright notice,
*    this  list of  conditions  and  the  disclaimer (as noted below)  in  the
*    documentation and/or other materials provided with the distribution.
*  - Neither the name of  the LLNS/LLNL nor the names of  its contributors may
*    be used to endorse or promote products derived from this software without
*    specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT  HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR  IMPLIED WARRANTIES, INCLUDING,  BUT NOT  LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND  FITNESS FOR A PARTICULAR  PURPOSE
* ARE  DISCLAIMED. IN  NO EVENT  SHALL LAWRENCE  LIVERMORE NATIONAL  SECURITY,
* LLC, THE  U.S.  DEPARTMENT OF  ENERGY  OR  CONTRIBUTORS BE  LIABLE  FOR  ANY
* DIRECT,  INDIRECT,   INCIDENTAL,   SPECIAL,   EXEMPLARY,  OR   CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT  LIMITED TO, PROCUREMENT OF  SUBSTITUTE GOODS OR
* SERVICES; LOSS OF  USE, DATA, OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER
* CAUSED  AND  ON  ANY  THEORY  OF  LIABILITY,  WHETHER  IN  CONTRACT,  STRICT
* LIABILITY, OR TORT  (INCLUDING NEGLIGENCE OR OTHERWISE)  ARISING IN ANY  WAY
* OUT OF THE  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
* DAMAGE.
*
*****************************************************************************/

// ************************************************************************* //
//                            avtM3DC1FileFormat.h                           //
// ************************************************************************* //

#ifndef AVT_M3DC1_FILE_FORMAT_H
#define AVT_M3DC1_FILE_FORMAT_H

#include <avtMTSDFileFormat.h>

#include <vtk_hdf5.h>

#include <vector>

class DBOptionsAttributes;
class vtkPoints;

// ****************************************************************************
//  Class: avtM3DC1FileFormat
//
//  Purpose:
//      Reads in M3DC1 files as a plugin to VisIt.
//
//  Programmer: allen -- generated by xml2avt
//  Creation:   Fri Dec 4 15:04:15 PST 2009
//
// ****************************************************************************

class avtM3DC1FileFormat : public avtMTSDFileFormat
{
  public:
                       avtM3DC1FileFormat(const char *, DBOptionsAttributes *);
    virtual           ~avtM3DC1FileFormat() {;};

    //
    // This is used to return unconvention data -- ranging from material
    // information to information about block connectivity.
    //
    // virtual void      *GetAuxiliaryData(const char *var, int timestep, 
    //                                     const char *type, void *args, 
    //                                     DestructorFunction &);
    //

    //
    // If you know the times and cycle numbers, overload this function.
    // Otherwise, VisIt will make up some reasonable ones for you.
    //
    virtual void        GetCycles(std::vector<int> &);
    virtual void        GetTimes(std::vector<double> &);

    virtual int            GetNTimesteps(void);

    virtual const char    *GetType(void)   { return "M3DC1"; };
    virtual void           FreeUpResources(void); 

    virtual bool CanCacheVariable(const char *var);
    virtual void RegisterDataSelections(const std::vector<avtDataSelection_p> &sels,
                                        std::vector<bool> *selectionsApplied);

    virtual bool ProcessDataSelections(int *mins, int *maxs, int *strides);

    virtual vtkDataSet    *GetMesh(int, const char *);
    virtual vtkDataArray  *GetVar(int, const char *);
    virtual vtkDataArray  *GetVectorVar(int, const char *);

  protected:
    vtkDataArray  *GetHeaderVar(int, const char *);
    vtkDataArray  *GetFieldVar(int, const char *);

    vtkPoints *GetMeshPoints(float *elements,
                             int refinementLevel);

    float * GetElements(int timestate, const char *meshname);

    void LoadFile();

    //HDF5 helper functions.
    bool ReadAttribute( hid_t parentID, const char *attr, void *value );
    bool ReadStringAttribute( hid_t parentID, const char *attr, std::string *value );
    hid_t NormalizeH5Type( hid_t type );

    // Some stuff to keep track of data selections
    std::vector<avtDataSelection_p> selList;
    std::vector<bool>              *selsApplied;

    bool processDataSelections;
    bool haveReadWholeData;

    // DATA MEMBERS
    hid_t m_fileID;
    std::string m_filename;
    int m_refinement;
    avtCentering m_dataLocation;

    std::vector<int>    m_cycles;
    std::vector<double> m_times;

public:
    std::vector<std::string> m_scalarVarNames;
    std::vector<std::string> m_fieldVarNames;

    // Variables read from mesh and field attributes.
    int nelms;
    int nvertices;
    int nplanes;

    int element_dimension;
    unsigned int element_size;
    unsigned int scalar_size;
    
  protected:

    virtual void PopulateDatabaseMetaData(avtDatabaseMetaData *, int);

    static herr_t linkIterator(hid_t, const char *, const H5L_info_t *, void *);
    static herr_t groupIterator(hid_t, const char *, void *);

};
#endif
