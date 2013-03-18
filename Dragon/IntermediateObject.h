#pragma once

#include "IntermediateCode.h"

struct IcFunctionObject;

enum IcObjectType
{
    IcNormalObjectType,
    IcImportObjectType
};

struct IcObject
{
    IcObjectReference Id;
    std::string Name;
    IcObjectType Type;
    union
    {
        struct
        {
            int Size;
            char *Data;
            IcFunctionObject *Function;
        } n;
        struct
        {
            std::string *ImportName;
        } i;
    };

    IcObject()
    {
        Id = 0;
        Type = (IcObjectType)0;
        memset(&this->n, 0, sizeof(this->n));
    }
};

struct IcFunctionObject
{
    IcType Type;
    IcBlock *StartBlock;
    IcBlock *EndBlock;
    std::set<IcBlock *> Blocks;
    std::map<IcLabelId, IcBlock *> BlockMap;
    std::map<int, IcFrameSlot> Slots;
};

class IcObjectManager
{
public:
    IcObjectManager()
        : _nextId(1)
    { }

    IcObject *CreateFunction(IcFunctionScope *scope);
    IcObject *CreateData(std::string *name, char *data, int size);
    IcObject *CreateImport(std::string &name);
    IcObject *FindObject(IcObjectReference id);
    IcObject *FindObject(std::string &name);
    std::pair<std::vector<IcObject *>::const_iterator, std::vector<IcObject *>::const_iterator> GetObjects();

private:
    IcObjectReference _nextId;
    std::vector<IcObject *> _objects;
    std::unordered_map<IcObjectReference, IcObject *> _objectsById;
    std::map<std::string, IcObject *> _objectsByName;

    void AddObject(IcObject *object);
};
