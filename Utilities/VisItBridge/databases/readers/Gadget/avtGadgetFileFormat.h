// Copyright (c) Lawrence Livermore National Security, LLC and other VisIt
// Project developers.  See the top-level LICENSE file for dates and other
// details.  No copyright assignment is required to contribute to VisIt.

// ************************************************************************* //
//                            avtGadgetFileFormat.h                           //
// ************************************************************************* //

#ifndef AVT_Gadget_FILE_FORMAT_H
#define AVT_Gadget_FILE_FORMAT_H

#include <avtSTSDFileFormat.h>
#define int4bytes int


// ****************************************************************************
//  Class: avtGadgetFileFormat
//
//  Purpose:
//      Reads in Gadget files as a plugin to VisIt.
//
//  Programmer: rwb -- generated by xml2avt
//  Creation:   Wed Oct 22 12:47:04 PDT 2008
//
// ****************************************************************************

class avtGadgetFileFormat : public avtSTSDFileFormat
{
  public:
                       avtGadgetFileFormat(const char *filename);
    virtual           ~avtGadgetFileFormat() {;};
    virtual const char    *GetType(void)   { return "Gadget"; };
    virtual void           FreeUpResources(void); 

    virtual vtkDataSet    *GetMesh(const char *);
    virtual vtkDataArray  *GetVar(const char *);
    virtual vtkDataArray  *GetVectorVar(const char *);
    virtual int GetCycle(void);
    virtual double GetTime(void);
    virtual int GetCycleFromFilename(const char *f) const
    {
      return GuessCycle(f);
    }
  protected:
    // DATA MEMBERS
    int4bytes blksize,swap;
    unsigned long ntot;
    double masstab[6],redshift,time;
    std::string fname;

    size_t my_fread(void *ptr, size_t size, size_t nmemb, FILE * stream);
    void swap_Nbyte(char *data,int n,int m);
    int get_block_names(FILE *fd, char **labels, int *vflag, int *numblocks);
    int find_block(FILE *fd,const char *label);
    int read_gadget_float3(float *data,const char *label,FILE *fd);
    int read_gadget_float(float *data,const char *label,FILE *fd);
    int read_gadget_head(int *npart,double *massarr,double *time,double *redshift,FILE *fd);
    virtual void PopulateDatabaseMetaData(avtDatabaseMetaData *);
};


#endif
