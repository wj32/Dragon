#pragma once

#include "IntermediateCode.h"
#include "IntermediateObject.h"

typedef std::pair<IcLabelId, IcLabelId> IcGraphEdge;
typedef std::map<IcLabelId, IcLabelId> IcLabelMap;
typedef std::map<IcLabelId, std::set<IcLabelId>> IcLabelMultimap;

struct IcGraph
{
    static void CreateGraph(IcFunctionObject *function, IcGraph &forward, IcGraph &backward);
    static void RemoveUnreachable(IcFunctionObject *function, IcGraph &forward, IcGraph &backward);
    static void Dominators(IcFunctionObject *function, IcGraph &forward, IcGraph &backward, IcLabelMultimap &dominators);
    static void ImmediateDominators(IcFunctionObject *function, IcGraph &forward, IcGraph &backward, IcLabelMultimap &dominators, IcLabelMap &immediateDominators);
    static void CreateDominatorTree(IcFunctionObject *function, IcGraph &graph);

    IcLabelMultimap Edges;
};
