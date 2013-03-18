#include "stdafx.h"
#include "IcGraph.h"

void IcGraph::CreateGraph(IcFunctionObject *function, IcGraph &forward, IcGraph &backward)
{
    for (auto it = function->Blocks.begin(); it != function->Blocks.end(); ++it)
    {
        IcBlock *block = *it;

        if (block != function->EndBlock)
        {
            assert(block->Operations.size() != 0);

            switch (block->Operations.back().Code)
            {
            case IcOpJmp:
                forward.Edges[block->Label].insert(block->Operations.back().b.B1->Label);
                backward.Edges[block->Operations.back().b.B1->Label].insert(block->Label);
                break;
            case IcOpBrE:
            case IcOpBrL:
            case IcOpBrLE:
                forward.Edges[block->Label].insert(block->Operations.back().rrbb.B3->Label);
                forward.Edges[block->Label].insert(block->Operations.back().rrbb.B4->Label);
                backward.Edges[block->Operations.back().rrbb.B3->Label].insert(block->Label);
                backward.Edges[block->Operations.back().rrbb.B4->Label].insert(block->Label);
                break;
            case IcOpBrEI:
            case IcOpBrLI:
            case IcOpBrLEI:
                forward.Edges[block->Label].insert(block->Operations.back().rcbb.B3->Label);
                forward.Edges[block->Label].insert(block->Operations.back().rcbb.B4->Label);
                backward.Edges[block->Operations.back().rcbb.B3->Label].insert(block->Label);
                backward.Edges[block->Operations.back().rcbb.B4->Label].insert(block->Label);
                break;
            default:
                assert(false);
            }
        }
    }
}

void IcGraph::RemoveUnreachable(IcFunctionObject *function, IcGraph &forward, IcGraph &backward)
{
    std::stack<IcBlock *> unseen;

    for (auto it = function->Blocks.begin(); it != function->Blocks.end(); ++it)
        unseen.push(*it);

    while (!unseen.empty())
    {
        IcBlock *block = unseen.top();

        unseen.pop();

        if (block == function->StartBlock || block == function->EndBlock)
            continue;

        if (backward.Edges.find(block->Label) == backward.Edges.end())
        {
            auto successorsIt = forward.Edges.find(block->Label);

            if (successorsIt != forward.Edges.end())
            {
                // We are deleting edges. Push the successors of the block back onto the unseen list.

                for (auto it = successorsIt->second.begin(); it != successorsIt->second.end(); ++it)
                    unseen.push(function->BlockMap[*it]);

                forward.Edges.erase(successorsIt);
            }

            for (auto it = backward.Edges.begin(); it != backward.Edges.end(); )
            {
                it->second.erase(block->Label);

                if (it->second.size() == 0)
                    backward.Edges.erase(it++);
                else
                    ++it;
            }            
        }
    }
}

void IcGraph::Dominators(IcFunctionObject *function, IcGraph &forward, IcGraph &backward, IcLabelMultimap &dominators)
{
    std::set<IcLabelId> initialSet;
    bool changed;

    for (auto it = function->BlockMap.begin(); it != function->BlockMap.end(); ++it)
        initialSet.insert(it->first);

    // Create the initial sets.

    dominators[function->StartBlock->Label].insert(function->StartBlock->Label);

    for (auto it = function->BlockMap.begin(); it != function->BlockMap.end(); ++it)
    {
        if (it->second != function->StartBlock)
            dominators[it->first] = initialSet;
    }

    do
    {
        changed = false;

        for (auto it = function->BlockMap.begin(); it != function->BlockMap.end(); ++it)
        {
            if (it->second != function->StartBlock)
            {
                std::set<IcLabelId> set;
                std::set<IcLabelId> newSet;
                std::set<IcLabelId>::iterator it2 = backward.Edges[it->first].begin();

                if (it2 != backward.Edges[it->first].end())
                {
                    set = dominators[*it2];
                    ++it2;
                }

                for (; it2 != backward.Edges[it->first].end(); ++it2)
                {
                    std::set<IcLabelId> &dom = dominators[*it2];

                    newSet.clear();
                    std::set_intersection(set.begin(), set.end(), dom.begin(), dom.end(), std::insert_iterator<std::set<IcLabelId>>(newSet, newSet.begin()));
                    set = newSet;
                }

                set.insert(it->first);

                if (dominators[it->first] != set)
                {
                    changed = true;
                    dominators[it->first] = set;
                }
            }
        }
    } while (changed);
}

void IcGraph::ImmediateDominators(IcFunctionObject *function, IcGraph &forward, IcGraph &backward, IcLabelMultimap &dominators, IcLabelMap &immediateDominators)
{
    std::stack<IcBlock *> unseen;
    std::set<IcBlock *> seen;

    // Note: the start block does not have any dominators.
    unseen.push(function->StartBlock);

    while (unseen.size() != 0)
    {
        IcBlock *block = unseen.top();

        unseen.pop();
        seen.insert(block);

        auto successorsIt = forward.Edges.find(block->Label);

        if (successorsIt != forward.Edges.end())
        {
            for (auto it = successorsIt->second.begin(); it != successorsIt->second.end(); ++it)
            {
                // If this successor is dominated by the current block, the current block is
                // its immediate dominator. Otherwise, the immediate dominator is the immediate
                // dominator of the current block.
                if (dominators[*it].find(block->Label) != dominators[*it].end())
                {
                    immediateDominators[*it] = block->Label;
                }
                else
                {
                    assert(block != function->StartBlock);
                    immediateDominators[*it] = immediateDominators[block->Label];
                }

                if (seen.find(function->BlockMap[*it]) == seen.end())
                    unseen.push(function->BlockMap[*it]);
            }
        }
    }
}

void IcGraph::CreateDominatorTree(IcFunctionObject *function, IcGraph &graph)
{
    
}
