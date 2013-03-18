#pragma once

#include "IntermediateCode.h"

class IcEvaluator
{
public:
    bool Execute(IcOperation &op);
    bool GetRegister(IcRegister r, IcImmediateValue &value);
    void SetRegister(IcRegister r, const IcImmediateValue &value);
    void Reset();

private:
    std::map<IcRegister, IcImmediateValue> _registers;

    bool IsThreeCode(IcOpcode opcode, bool *immediate = NULL);
    bool IsChXXX(IcOpcode opcode, bool *immediate = NULL);
};
