#include "stdafx.h"
#include "IntermediateObject.h"

IcObject *IcObjectManager::CreateFunction(IcFunctionScope *scope)
{
    IcObject *object = new IcObject;
    IcFunctionObject *function = new IcFunctionObject;

    function->Type = scope->Type;
    function->StartBlock = scope->StartBlock;
    function->EndBlock = scope->EndBlock;
    function->Blocks = scope->Blocks;
    function->BlockMap = scope->BlockMap;
    function->Slots = scope->Slots;

    object->Id = _nextId++;
    object->Name = scope->Symbol->Name;
    object->Type = IcNormalObjectType;
    object->n.Size = -1;
    object->n.Function = function;

    AddObject(object);

    return object;
}

IcObject *IcObjectManager::CreateData(std::string *name, char *data, int size)
{
    IcObject *object = new IcObject;

    object->Id = _nextId++;
    object->Type = IcNormalObjectType;
    object->n.Size = size;
    object->n.Data = data;

    if (name)
        object->Name = *name;

    AddObject(object);

    return object;
}

IcObject *IcObjectManager::CreateImport(std::string &name)
{
    IcObject *object = new IcObject;

    object->Id = _nextId++;
    object->Type = IcImportObjectType;
    object->i.ImportName = new std::string(name);

    AddObject(object);

    return object;
}

IcObject *IcObjectManager::FindObject(IcObjectReference id)
{
    auto it = _objectsById.find(id);

    if (it != _objectsById.end())
        return it->second;
    else
        return NULL;
}

IcObject *IcObjectManager::FindObject(std::string &name)
{
    auto it = _objectsByName.find(name);

    if (it != _objectsByName.end())
        return it->second;
    else
        return NULL;
}

std::pair<std::vector<IcObject *>::const_iterator, std::vector<IcObject *>::const_iterator> IcObjectManager::GetObjects()
{
    return std::pair<std::vector<IcObject *>::const_iterator, std::vector<IcObject *>::const_iterator>(_objects.begin(), _objects.end());
}

void IcObjectManager::AddObject(IcObject *object)
{
    _objects.push_back(object);
    _objectsById[object->Id] = object;

    if (object->Name.length() != 0)
    {
        if (_objectsByName.find(object->Name) != _objectsByName.end())
            throw "object name already in use";

        _objectsByName[object->Name] = object;
    }
}
