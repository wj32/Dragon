#include "stdafx.h"
#include "IntermediateCode.h"

int IcNextBlockId = 1;

int IcFunctionScope::AllocateSlot(int size, IcVariableId variableId)
{
    IcFrameSlot slot;

    slot.Size = size;
    slot.VariableId = variableId;

    int slotId = NextSlotId++;

    Slots[slotId] = slot;

    return slotId;
}

IcBlock *IcFunctionScope::CreateBlock()
{
    IcBlock *block = new IcBlock;

    block->Label = IcBlock::AllocateLabelId();
    Blocks.insert(block);
    BlockMap[block->Label] = block;

    return block;
}
