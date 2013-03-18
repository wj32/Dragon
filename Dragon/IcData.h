#pragma once

#include "IntermediateCode.h"

enum IcDataOperationSelector
{
    IcSelectorNone,
    IcSelectorR,
    IcSelectorRR,
    IcSelectorCR,
    IcSelectorRRR,
    IcSelectorRRC,
    IcSelectorRCR,
    IcSelectorRRBB,
    IcSelectorRCBB,
    IcSelectorRRCCR,
    IcSelectorRCCCR,
    IcSelectorB,
    IcSelectorRRSRX,
    IcSelectorCRSRX,
    IcSelectorRSRX
};

struct IcDataOperationData
{
    IcOpcode Code;
    char *Name;
    IcDataOperationSelector Selector;
    bool Output;
};

extern IcDataOperationData IcOpData[IcMaximumOpcode];
