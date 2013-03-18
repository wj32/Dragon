#include "stdafx.h"
#include "IcEvaluator.h"

#define EXECUTE_THREE_CODE(mode, in1, op, in2, out, typeKeyword) do { \
    out.Type = mode; \
    switch (mode) \
    { \
    case IcCharType: out.u.Char = ((typeKeyword char)in1.u.Char) op ((typeKeyword char)in2.u.Char); break; \
    case IcShortType: out.u.Short = ((typeKeyword short)in1.u.Short) op ((typeKeyword short)in2.u.Short); break; \
    case IcIntType: out.u.Int = ((typeKeyword int)in1.u.Int) op ((typeKeyword int)in2.u.Int); break; \
    case IcLongType: out.u.Long = ((typeKeyword long)in1.u.Long) op ((typeKeyword long)in2.u.Long); break; \
    case IcIntPtrType: out.u.IntPtr = ((typeKeyword int)in1.u.IntPtr) op ((typeKeyword int)in2.u.IntPtr); break; \
    default: throw "invalid type"; break; \
    } } while (0)

#define EXECUTE_TWO_CODE(mode, op, in, out, typeKeyword) do { \
    out.Type = mode; \
    switch (mode) \
    { \
    case IcCharType: out.u.Char = op ((typeKeyword char)in.u.Char); break; \
    case IcShortType: out.u.Short = op ((typeKeyword short)in.u.Short); break; \
    case IcIntType: out.u.Int = op ((typeKeyword int)in.u.Int); break; \
    case IcLongType: out.u.Long = op ((typeKeyword long)in.u.Long); break; \
    case IcIntPtrType: out.u.IntPtr = op ((typeKeyword int)in.u.IntPtr); break; \
    default: throw "invalid type"; break; \
    } } while (0)

#define EXECUTE_CVT(mode, in, out, typeKeyword) do { \
    out.Type = mode; \
    switch (mode) \
    { \
    case IcCharType: out.u.Char = (char)((typeKeyword)in.u.Long); break; \
    case IcShortType: out.u.Short = (short)((typeKeyword)in.u.Long); break; \
    case IcIntType: out.u.Int = (int)((typeKeyword)in.u.Long); break; \
    case IcLongType: out.u.Long = (long)((typeKeyword)in.u.Long); break; \
    case IcIntPtrType: out.u.IntPtr = (int)((typeKeyword)in.u.Long); break; \
    default: throw "invalid type"; break; \
    } } while (0)

#define EXECUTE_CH(mode, in1, op, in2, in3, in4, out, typeKeyword) do { \
    out.Type = IcIntType; \
    switch (mode) \
    { \
    case IcCharType: out.u.Int = ((typeKeyword char)in1.u.Char) op ((typeKeyword char)in2.u.Char) ? in3.u.Int : in4.u.Int; break; \
    case IcShortType: out.u.Int = ((typeKeyword short)in1.u.Short) op ((typeKeyword short)in2.u.Short) ? in3.u.Int : in4.u.Int; break; \
    case IcIntType: out.u.Int = ((typeKeyword int)in1.u.Int) op ((typeKeyword int)in2.u.Int) ? in3.u.Int : in4.u.Int; break; \
    case IcLongType: out.u.Int = ((typeKeyword long)in1.u.Long) op ((typeKeyword long)in2.u.Long) ? in3.u.Int : in4.u.Int; break; \
    case IcIntPtrType: out.u.Int = ((typeKeyword int)in1.u.IntPtr) op ((typeKeyword int)in2.u.IntPtr) ? in3.u.Int : in4.u.Int; break; \
    default: throw "invalid type"; break; \
    } } while (0)

bool IcEvaluator::Execute(IcOperation &op)
{
    IcImmediateValue value1;
    IcImmediateValue value2;
    IcImmediateValue newValue;
    bool immediate;

    memset(&newValue, 0, sizeof(IcImmediateValue));

    if (op.Mode == IcFloatType || op.Mode == IcDoubleType)
        return false;

    if (IsThreeCode(op.Code, &immediate))
    {
        IcRegister output;

        if (immediate)
        {
            if (!GetRegister(op.rcr.R1, value1))
                return false;

            value2 = op.rcr.C2;
            output = op.rcr.R3;

            if (value2.Type == IcReferenceType)
                return false;
        }
        else
        {
            if (!GetRegister(op.rrr.R1, value1))
                return false;
            if (!GetRegister(op.rrr.R2, value2))
                return false;

            output = op.rrr.R3;
        }

        switch (op.Code)
        {
        case IcOpAdd:
        case IcOpAddI:
            EXECUTE_THREE_CODE(op.Mode, value1, +, value2, newValue, signed);
            break;
        case IcOpSub:
        case IcOpSubI:
            EXECUTE_THREE_CODE(op.Mode, value1, -, value2, newValue, signed);
            break;
        case IcOpSubIR:
            EXECUTE_THREE_CODE(op.Mode, value2, -, value1, newValue, signed);
            break;
        case IcOpMul:
        case IcOpMulI:
            EXECUTE_THREE_CODE(op.Mode, value1, *, value2, newValue, signed);
            break;
        case IcOpMulU:
        case IcOpMulUI:
            EXECUTE_THREE_CODE(op.Mode, value1, *, value2, newValue, unsigned);
            break;
        case IcOpDiv:
        case IcOpDivI:
            EXECUTE_THREE_CODE(op.Mode, value1, /, value2, newValue, signed);
            break;
        case IcOpDivIR:
            EXECUTE_THREE_CODE(op.Mode, value2, /, value1, newValue, signed);
            break;
        case IcOpDivU:
        case IcOpDivUI:
            EXECUTE_THREE_CODE(op.Mode, value1, /, value2, newValue, unsigned);
            break;
        case IcOpDivUIR:
            EXECUTE_THREE_CODE(op.Mode, value2, /, value1, newValue, unsigned);
            break;
        case IcOpMod:
        case IcOpModI:
            EXECUTE_THREE_CODE(op.Mode, value1, %, value2, newValue, signed);
            break;
        case IcOpModU:
        case IcOpModUI:
            EXECUTE_THREE_CODE(op.Mode, value1, %, value2, newValue, unsigned);
            break;
        case IcOpAnd:
        case IcOpAndI:
            EXECUTE_THREE_CODE(op.Mode, value1, &, value2, newValue, unsigned);
            break;
        case IcOpOr:
        case IcOpOrI:
            EXECUTE_THREE_CODE(op.Mode, value1, |, value2, newValue, unsigned);
            break;
        case IcOpXor:
        case IcOpXorI:
            EXECUTE_THREE_CODE(op.Mode, value1, ^, value2, newValue, unsigned);
            break;
        case IcOpShl:
            EXECUTE_THREE_CODE(op.Mode, value1, <<, value2, newValue, signed);
            break;
        case IcOpShlU:
            EXECUTE_THREE_CODE(op.Mode, value1, <<, value2, newValue, unsigned);
            break;
        case IcOpShr:
            EXECUTE_THREE_CODE(op.Mode, value1, >>, value2, newValue, signed);
            break;
        case IcOpShrU:
            EXECUTE_THREE_CODE(op.Mode, value1, >>, value2, newValue, unsigned);
            break;
        }

        _registers[output] = newValue;

        return true;
    }

    switch (op.Code)
    {
    case IcOpMov:
        {
            if (!GetRegister(op.rr.R1, value1))
                return false;

            _registers[op.rr.R2] = value1;

            return true;
        }
        break;
    case IcOpCvtC:
    case IcOpCvtUC:
    case IcOpCvtS:
    case IcOpCvtUS:
    case IcOpCvtI:
    case IcOpCvtUI:
    case IcOpCvtL:
    case IcOpCvtUL:
    case IcOpCvtLL:
    case IcOpCvtULL:
    case IcOpCvtP:
        {
            if (!GetRegister(op.rr.R1, value1))
                return false;

            switch (op.Code)
            {
            case IcOpCvtC:
                EXECUTE_CVT(op.Mode, value1, newValue, signed char);
                break;
            case IcOpCvtUC:
                EXECUTE_CVT(op.Mode, value1, newValue, unsigned char);
                break;
            case IcOpCvtS:
                EXECUTE_CVT(op.Mode, value1, newValue, signed short);
                break;
            case IcOpCvtUS:
                EXECUTE_CVT(op.Mode, value1, newValue, unsigned short);
                break;
            case IcOpCvtI:
                EXECUTE_CVT(op.Mode, value1, newValue, signed int);
                break;
            case IcOpCvtUI:
                EXECUTE_CVT(op.Mode, value1, newValue, unsigned int);
                break;
            case IcOpCvtL:
                EXECUTE_CVT(op.Mode, value1, newValue, signed long);
                break;
            case IcOpCvtUL:
                EXECUTE_CVT(op.Mode, value1, newValue, unsigned long);
                break;
            case IcOpCvtLL:
                EXECUTE_CVT(op.Mode, value1, newValue, signed long long);
                break;
            case IcOpCvtULL:
                EXECUTE_CVT(op.Mode, value1, newValue, unsigned long long);
                break;
            case IcOpCvtP:
                EXECUTE_CVT(op.Mode, value1, newValue, signed int);
                break;
            }

            _registers[op.rr.R2] = newValue;

            return true;
        }
        break;
    case IcOpNot:
        {
            if (!GetRegister(op.rr.R1, value1))
                return false;

            EXECUTE_TWO_CODE(op.Mode, ~, value1, newValue, unsigned);
            _registers[op.rr.R2] = newValue;

            return true;
        }
        break;
    case IcOpLdI:
        {
            if (op.cr.C1.Type == IcReferenceType)
                return false;

            _registers[op.cr.R2] = op.cr.C1;

            return true;
        }
        break;
    }

    if (IsChXXX(op.Code, &immediate))
    {
        IcImmediateValue value3;
        IcImmediateValue value4;
        IcRegister output;

        if (immediate)
        {
            if (!GetRegister(op.rcccr.R1, value1))
                return false;

            value2 = op.rcccr.C2;
            value3 = op.rcccr.C3;
            value4 = op.rcccr.C4;
            output = op.rcccr.R5;

            if (value2.Type == IcReferenceType)
                return false;
            if (value3.Type == IcReferenceType)
                return false;
            if (value4.Type == IcReferenceType)
                return false;
        }
        else
        {
            if (!GetRegister(op.rrccr.R1, value1))
                return false;
            if (!GetRegister(op.rrccr.R2, value2))
                return false;

            value3 = op.rrccr.C3;
            value4 = op.rrccr.C4;
            output = op.rrccr.R5;

            if (value3.Type == IcReferenceType)
                return false;
            if (value4.Type == IcReferenceType)
                return false;
        }

        switch (op.Code)
        {
        case IcOpChE:
            EXECUTE_CH(op.Mode, value1, ==, value2, value3, value4, newValue, signed);
            break;
        case IcOpChL:
            EXECUTE_CH(op.Mode, value1, <, value2, value3, value4, newValue, signed);
            break;
        case IcOpChLE:
            EXECUTE_CH(op.Mode, value1, <=, value2, value3, value4, newValue, signed);
            break;
        case IcOpChB:
            EXECUTE_CH(op.Mode, value1, <, value2, value3, value4, newValue, unsigned);
            break;
        case IcOpChBE:
            EXECUTE_CH(op.Mode, value1, <=, value2, value3, value4, newValue, unsigned);
            break;
        }

        _registers[output] = newValue;

        return true;
    }

    return false;
}

bool IcEvaluator::GetRegister(IcRegister r, IcImmediateValue &value)
{
    auto it = _registers.find(r);

    if (it != _registers.end())
    {
        value = it->second;
        return true;
    }

    return false;
}

void IcEvaluator::SetRegister(IcRegister r, const IcImmediateValue &value)
{
    _registers[r] = value;
}

void IcEvaluator::Reset()
{
    _registers.clear();
}

bool IcEvaluator::IsThreeCode(IcOpcode opcode, bool *immediate)
{
    if (immediate)
        *immediate = false;

    switch (opcode)
    {
    case IcOpAdd:
    case IcOpSub:
    case IcOpMul:
    case IcOpMulU:
    case IcOpDiv:
    case IcOpDivU:
    case IcOpMod:
    case IcOpModU:
    case IcOpAnd:
    case IcOpOr:
    case IcOpXor:
    case IcOpShl:
    case IcOpShlU:
    case IcOpShr:
    case IcOpShrU:
        return true;
    }

    switch (opcode)
    {
    case IcOpAddI:
    case IcOpSubI:
    case IcOpSubIR:
    case IcOpMulI:
    case IcOpMulUI:
    case IcOpDivI:
    case IcOpDivIR:
    case IcOpDivUI:
    case IcOpDivUIR:
    case IcOpModI:
    case IcOpModUI:
    case IcOpAndI:
    case IcOpOrI:
    case IcOpXorI:
        if (immediate)
            *immediate = true;
        return true;
    }

    return false;
}

bool IcEvaluator::IsChXXX(IcOpcode opcode, bool *immediate)
{
    if (immediate)
        *immediate = false;

    switch (opcode)
    {
    case IcOpChE:
    case IcOpChL:
    case IcOpChLE:
    case IcOpChB:
    case IcOpChBE:
        return true;
    }

    switch (opcode)
    {
    case IcOpChEI:
    case IcOpChLI:
    case IcOpChLEI:
    case IcOpChBI:
    case IcOpChBEI:
        if (immediate)
            *immediate = true;
        return true;
    }

    return false;
}
