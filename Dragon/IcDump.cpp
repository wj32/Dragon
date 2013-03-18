#include "stdafx.h"
#include "IcDump.h"
#include "IcData.h"

void IcDump::DumpFunction(std::string &name, IcFunctionObject *object)
{
    std::queue<IcBlock *> queue;
    std::set<IcBlock *> done;

    _output << name << std::endl;

    for (auto it = object->Slots.begin(); it != object->Slots.end(); ++it)
    {
        _output << "    Slot " << it->first << ": " << it->second.Size << " bytes" << std::endl;
    }

    _output << "{" << std::endl;

    _nextLabel = 1;
    queue.push(object->StartBlock);
    done.insert(object->StartBlock);

    while (!queue.empty())
    {
        IcBlock *block = queue.front();

        queue.pop();

        DumpLabel(block);
        _output << ":" << std::endl;
        DumpBlock(block);

        if (block->Operations.size() != 0)
        {
            IcOperation &op = block->Operations.back();

            switch (IcOpData[op.Code].Selector)
            {
            case IcSelectorB:
                if (done.find(op.b.B1) == done.end())
                {
                    queue.push(op.b.B1);
                    done.insert(op.b.B1);
                }
                break;
            case IcSelectorRRBB:
                if (done.find(op.rrbb.B3) == done.end())
                {
                    queue.push(op.rrbb.B3);
                    done.insert(op.rrbb.B3);
                }
                if (done.find(op.rrbb.B4) == done.end())
                {
                    queue.push(op.rrbb.B4);
                    done.insert(op.rrbb.B4);
                }
                break;
            case IcSelectorRCBB:
                if (done.find(op.rcbb.B3) == done.end())
                {
                    queue.push(op.rcbb.B3);
                    done.insert(op.rcbb.B3);
                }
                if (done.find(op.rcbb.B4) == done.end())
                {
                    queue.push(op.rcbb.B4);
                    done.insert(op.rcbb.B4);
                }
                break;
            }
        }
    }

    _output << "}" << std::endl;
}

void IcDump::DumpBlock(IcBlock *block)
{
    for (auto it = block->PhiOperations.begin(); it != block->PhiOperations.end(); ++it)
    {
        _output << "    ";
        DumpOperation(*it);
        _output << std::endl;
    }

    for (auto it = block->Operations.begin(); it != block->Operations.end(); ++it)
    {
        _output << "    ";
        DumpOperation(*it);
        _output << std::endl;
    }
}

void IcDump::DumpOperation(IcOperation &op)
{
    IcDataOperationData &data = IcOpData[op.Code];

    _output << data.Name;

    if (op.Mode != IcInvalidType)
    {
        _output << "<" << DumpBaseType(op.Mode) << ">";
    }

    if (data.Selector != IcSelectorNone)
    {
        _output << " ";

        switch (data.Selector)
        {
        case IcSelectorR:
            if (data.Output) _output << "-> ";
            DumpRegister(op.r.R1);
            break;
        case IcSelectorRR:
            DumpRegister(op.rr.R1);
            if (data.Output) _output << " -> "; else _output << ", ";
            DumpRegister(op.rr.R2);
            break;
        case IcSelectorCR:
            DumpImmediateValue(op.cr.C1);
            if (data.Output) _output << " -> "; else _output << ", ";
            DumpRegister(op.cr.R2);
            break;
        case IcSelectorRRR:
            DumpRegister(op.rrr.R1);
            _output << ", ";
            DumpRegister(op.rrr.R2);
            if (data.Output) _output << " -> "; else _output << ", ";
            DumpRegister(op.rrr.R3);
            break;
        case IcSelectorRRC:
            DumpRegister(op.rrc.R1);
            _output << ", ";
            DumpRegister(op.rrc.R2);
            _output << ", ";
            DumpImmediateValue(op.rrc.C3);
            break;
        case IcSelectorRCR:
            DumpRegister(op.rcr.R1);
            _output << ", ";
            DumpImmediateValue(op.rcr.C2);
            if (data.Output) _output << " -> "; else _output << ", ";
            DumpRegister(op.rcr.R3);
            break;
        case IcSelectorRRBB:
            DumpRegister(op.rrbb.R1);
            _output << ", ";
            DumpRegister(op.rrbb.R2);
            _output << ", ";
            DumpLabel(op.rrbb.B3);
            _output << ", ";
            DumpLabel(op.rrbb.B4);
            break;
        case IcSelectorRCBB:
            DumpRegister(op.rcbb.R1);
            _output << ", ";
            DumpImmediateValue(op.rcbb.C2);
            _output << ", ";
            DumpLabel(op.rcbb.B3);
            _output << ", ";
            DumpLabel(op.rcbb.B4);
            break;
        case IcSelectorRRCCR:
            DumpRegister(op.rrccr.R1);
            _output << ", ";
            DumpRegister(op.rrccr.R2);
            _output << ", ";
            DumpImmediateValue(op.rrccr.C3);
            _output << ", ";
            DumpImmediateValue(op.rrccr.C4);
            if (data.Output) _output << " -> "; else _output << ", ";
            DumpRegister(op.rrccr.R5);
            break;
        case IcSelectorRCCCR:
            DumpRegister(op.rcccr.R1);
            _output << ", ";
            DumpImmediateValue(op.rcccr.C2);
            _output << ", ";
            DumpImmediateValue(op.rcccr.C3);
            _output << ", ";
            DumpImmediateValue(op.rcccr.C4);
            if (data.Output) _output << " -> "; else _output << ", ";
            DumpRegister(op.rcccr.R5);
            break;
        case IcSelectorB:
            DumpLabel(op.b.B1);
            break;
        case IcSelectorRRSRX:
            DumpRegister(op.rrsrx.R1);

            if (op.rrsrx.Rs)
            {
                for (auto it = op.rrsrx.Rs->begin(); it != op.rrsrx.Rs->end(); ++it)
                {
                    _output << ", ";
                    DumpRegister(*it);
                }
            }

            if (data.Output)
            {
                _output << " -> ";
                DumpRegister(op.rrsrx.RX);
            }

            break;
        case IcSelectorCRSRX:
            DumpImmediateValue(op.crsrx.C1);

            if (op.crsrx.Rs)
            {
                for (auto it = op.crsrx.Rs->begin(); it != op.crsrx.Rs->end(); ++it)
                {
                    _output << ", ";
                    DumpRegister(*it);
                }
            }

            if (data.Output)
            {
                _output << " -> ";
                DumpRegister(op.crsrx.RX);
            }

            break;
        case IcSelectorRSRX:
            if (op.rsrx.Rs)
            {
                for (auto it = op.rsrx.Rs->begin(); it != op.rsrx.Rs->end(); ++it)
                {
                    if (it != op.rsrx.Rs->begin())
                        _output << ", ";

                    DumpRegister(*it);
                }
            }

            _output << " -> ";
            DumpRegister(op.rsrx.RX);

            break;
        }
    }
}

void IcDump::DumpRegister(IcRegister r)
{
    _output << "r" << (int)r;
}

void IcDump::DumpImmediateValue(IcImmediateValue &immediate)
{
    if (immediate.Type == IcReferenceType)
    {
        _output << "[ref:" << (int)immediate.u.Reference << "]";

        if (immediate.u.ReferenceOffset != 0)
        {
            _output << "+0x";
            _output.setf(std::ios_base::hex, std::ios_base::dec);
            _output << immediate.u.ReferenceOffset;
            _output.setf(std::ios_base::dec, std::ios_base::hex);
        }
    }
    else
    {
        switch (immediate.Type)
        {
        case IcCharType:
            _output << "(Char)" << (int)immediate.u.Char;
            if (isprint(immediate.u.Char))
                _output << ":'" << immediate.u.Char << "'";
            break;
        case IcShortType:
            _output << "(Short)" << immediate.u.Short;
            if (isprint(immediate.u.Short))
                _output << ":'" << (char)immediate.u.Short << "'";
            break;
        case IcIntType: _output << "(Int)" << immediate.u.Int; break;
        case IcLongType: _output << "(Long)" << immediate.u.Long; break;
        case IcLongLongType: _output << "(LongLong)" << immediate.u.Long; break; // invalid
        case IcFloatType: _output << "(Float)" << immediate.u.Float; break;
        case IcDoubleType: _output << "(Double)" << immediate.u.Double; break;
        case IcIntPtrType: _output << "(IntPtr)" << immediate.u.IntPtr; break;
        }
    }
}

void IcDump::DumpLabel(IcBlock *block)
{
    auto it = _labels.find(block);

    if (it != _labels.end())
    {
        _output << it->second;
        return;
    }

    std::ostringstream label;

    label << "Block";
    label << _nextLabel++;

    _labels.insert(std::pair<IcBlock *, std::string>(block, label.str()));
    _output << label.str();
}

char *IcDump::DumpBaseType(IcBaseType baseType)
{
    switch (baseType)
    {
    case IcCharType: return "Char";
    case IcShortType: return "Short";
    case IcIntType: return "Int";
    case IcLongType: return "Long";
    case IcLongLongType: return "LongLong";
    case IcFloatType: return "Float";
    case IcDoubleType: return "Double";
    case IcIntPtrType: return "IntPtr";
    case IcVoidType: return "Void";
    case IcReferenceType: return "Reference";
    default: return NULL;
    }
}
