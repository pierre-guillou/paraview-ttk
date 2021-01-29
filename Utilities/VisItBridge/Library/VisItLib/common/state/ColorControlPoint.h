// Copyright (c) Lawrence Livermore National Security, LLC and other VisIt
// Project developers.  See the top-level LICENSE file for dates and other
// details.  No copyright assignment is required to contribute to VisIt.

#ifndef COLORCONTROLPOINT_H
#define COLORCONTROLPOINT_H
#include <state_exports.h>
#include <AttributeSubject.h>


// ****************************************************************************
// Class: ColorControlPoint
//
// Purpose:
//    This class contains an RGBA color with a position value.
//
// Notes:      Autogenerated by xml2atts.
//
// Programmer: xml2atts
// Creation:   omitted
//
// Modifications:
//
// ****************************************************************************

class STATE_API ColorControlPoint : public AttributeSubject
{
public:
    // These constructors are for objects of this class
    ColorControlPoint();
    ColorControlPoint(const ColorControlPoint &obj);
protected:
    // These constructors are for objects derived from this class
    ColorControlPoint(private_tmfs_t tmfs);
    ColorControlPoint(const ColorControlPoint &obj, private_tmfs_t tmfs);
public:
    virtual ~ColorControlPoint();

    virtual ColorControlPoint& operator = (const ColorControlPoint &obj);
    virtual bool operator == (const ColorControlPoint &obj) const;
    virtual bool operator != (const ColorControlPoint &obj) const;
private:
    void Init();
    void Copy(const ColorControlPoint &obj);
public:

    virtual const std::string TypeName() const;
    virtual bool CopyAttributes(const AttributeGroup *);
    virtual AttributeSubject *CreateCompatible(const std::string &) const;
    virtual AttributeSubject *NewInstance(bool) const;

    // Property selection methods
    virtual void SelectAll();
    void SelectColors();

    // Property setting methods
    void SetColors(const unsigned char *colors_);
    void SetPosition(float position_);

    // Property getting methods
    const unsigned char *GetColors() const;
          unsigned char *GetColors();
    float               GetPosition() const;

    // Persistence methods
    virtual bool CreateNode(DataNode *node, bool completeSave, bool forceAdd);
    virtual void SetFromNode(DataNode *node);


    // Keyframing methods
    virtual std::string               GetFieldName(int index) const;
    virtual AttributeGroup::FieldType GetFieldType(int index) const;
    virtual std::string               GetFieldTypeName(int index) const;
    virtual bool                      FieldsEqual(int index, const AttributeGroup *rhs) const;

    // User-defined methods
    ColorControlPoint(float pos, unsigned char r, unsigned char g, unsigned char b, unsigned char a);

    // IDs that can be used to identify fields in case statements
    enum {
        ID_colors = 0,
        ID_position,
        ID__LAST
    };

private:
    unsigned char colors[4];
    float         position;

    // Static class format string for type map.
    static const char *TypeMapFormatString;
    static const private_tmfs_t TmfsStruct;
};
#define COLORCONTROLPOINT_TMFS "Uf"

#endif
