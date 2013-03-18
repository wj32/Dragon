#pragma once

struct IcStructType;
struct IcEnumType;
struct IcFunctionType;
struct IcBlock;
struct IcOrdinarySymbol;

typedef int IcObjectReference;
typedef int IcVariableId;

bool EqualToStruct(IcStructType *type1, IcStructType *type2);
int SizeOfStruct(IcStructType *type);
int AlignOfStruct(IcStructType *type);
bool EqualToFunction(IcFunctionType *type1, IcFunctionType *type2);

enum IcBaseType
{
    IcInvalidType,
    IcCharType,
    IcShortType,
    IcIntType,
    IcLongType,
    IcLongLongType,
    IcFloatType,
    IcDoubleType,
    IcIntPtrType,
    IcVoidType,
    IcReferenceType
};

enum IcTypeTraits
{
    //IcSignedTrait = 0x1,
    IcUnsignedTrait = 0x2,
    IcVolatileTrait = 0x4,
    IcConstTrait = 0x8
};

struct IcImmediateValue
{
    IcBaseType Type;

    union
    {
        char Char;
        short Short;
        int Int;
        long Long;
        float Float;
        double Double;
        int IntPtr;
        struct
        {
            IcObjectReference Reference;
            int ReferenceOffset;
        };
    } u;
};

typedef unsigned int IcRegister;
typedef std::vector<IcRegister> IcRegisterList;

enum IcOpcode
{
    IcOpMov = 1, // rr
    IcOpCvtC, // rr
    IcOpCvtUC, // rr
    IcOpCvtS, // rr
    IcOpCvtUS, // rr
    IcOpCvtI, // rr
    IcOpCvtUI, // rr
    IcOpCvtL, // rr
    IcOpCvtUL, // rr
    IcOpCvtLL, // rr
    IcOpCvtULL, // rr
    IcOpCvtF, // rr
    IcOpCvtD, // rr
    IcOpCvtP, // rr
    IcOpAdd, // rrr
    IcOpAddI, // rcr
    IcOpSub, // rrr
    IcOpSubI, // rcr
    IcOpSubIR, // rcr
    IcOpMul, // rrr
    IcOpMulI, // rcr
    IcOpMulU, // rrr
    IcOpMulUI, // rcr
    IcOpDiv, // rrr
    IcOpDivI, // rcr
    IcOpDivIR, // rcr
    IcOpDivU, // rrr
    IcOpDivUI, // rcr
    IcOpDivUIR, // rcr
    IcOpMod, // rrr
    IcOpModI, // rcr
    IcOpModU, // rrr
    IcOpModUI, // rcr
    IcOpNot, // rr
    IcOpAnd, // rrr
    IcOpAndI, // rcr
    IcOpOr, // rrr
    IcOpOrI, // rcr
    IcOpXor, // rrr
    IcOpXorI, // rcr
    IcOpShl, // rrr
    IcOpShlU, // rrr
    IcOpShr, // rrr
    IcOpShrU, // rrr
    IcOpLd, // rr
    IcOpLdI, // cr
    IcOpLdF, // cr
    IcOpLdFA, // r
    IcOpLdN, // r
    IcOpSt, // rr
    IcOpStI, // cr
    IcOpSrt, // r
    IcOpCpyI, // rrc
    IcOpJmp, // b
    IcOpBrE, // rrbb
    IcOpBrL, // rrbb
    IcOpBrLE, // rrbb
    IcOpBrB, // rrbb
    IcOpBrBE, // rrbb
    IcOpBrEI, // rcbb
    IcOpBrLI, // rcbb
    IcOpBrLEI, // rcbb
    IcOpBrBI, // rcbb
    IcOpBrBEI, // rcbb
    IcOpChE, // rrccr
    IcOpChL, // rrccr
    IcOpChLE, // rrccr
    IcOpChB, // rrccr
    IcOpChBE, // rrccr
    IcOpChEI, // rcccr
    IcOpChLI, // rcccr
    IcOpChLEI, // rcccr
    IcOpChBI, // rcccr
    IcOpChBEI, // rcccr
    IcOpCall, // rrsrx
    IcOpCallV, // rrsrx
    IcOpCallI, // crsrx
    IcOpCallVI, // crsrx
    IcOpPhi, // rsrx
    IcOpEnd,

    IcMaximumOpcode
};

struct IcOperation
{
    IcOpcode Code;
    IcBaseType Mode;
    union
    {
        struct
        {
            IcRegister R1;
        } r;
        struct
        {
            IcRegister R1;
            IcRegister R2;
        } rr;
        struct
        {
            IcImmediateValue C1;
            IcRegister R2;
        } cr;
        struct
        {
            IcRegister R1;
            IcRegister R2;
            IcRegister R3;
        } rrr;
        struct
        {
            IcRegister R1;
            IcRegister R2;
            IcImmediateValue C3;
        } rrc;
        struct
        {
            IcRegister R1;
            IcImmediateValue C2;
            IcRegister R3;
        } rcr;
        struct
        {
            IcRegister R1;
            IcRegister R2;
            IcBlock *B3;
            IcBlock *B4;
        } rrbb;
        struct
        {
            IcRegister R1;
            IcImmediateValue C2;
            IcBlock *B3;
            IcBlock *B4;
        } rcbb;
        struct
        {
            IcRegister R1;
            IcRegister R2;
            IcImmediateValue C3;
            IcImmediateValue C4;
            IcRegister R5;
        } rrccr;
        struct
        {
            IcRegister R1;
            IcImmediateValue C2;
            IcImmediateValue C3;
            IcImmediateValue C4;
            IcRegister R5;
        } rcccr;
        struct
        {
            IcBlock *B1;
        } b;
        struct
        {
            IcRegister R1;
            IcRegisterList *Rs;
            IcRegister RX;
        } rrsrx;
        struct
        {
            IcImmediateValue C1;
            IcRegisterList *Rs;
            IcRegister RX;
        } crsrx;
        struct
        {
            IcRegisterList *Rs;
            IcRegister RX;
        } rsrx;
    };

    IcOperation()
    {
        memset(this, 0, sizeof(IcOperation));
    }

    IcOperation(const IcOperation &other)
    {
        *this = other;
    }

    ~IcOperation()
    {
        if (Code == IcOpCall || Code == IcOpCallV)
        {
            if (rrsrx.Rs)
                delete rrsrx.Rs;
        }
        else if (Code == IcOpCallI || Code == IcOpCallVI)
        {
            if (crsrx.Rs)
                delete crsrx.Rs;
        }
        else if (Code == IcOpPhi)
        {
            if (rsrx.Rs)
                delete rsrx.Rs;
        }
    }

    IcOperation &operator=(const IcOperation &other)
    {
        memcpy(this, &other, sizeof(IcOperation));

        if (Code == IcOpCall || Code == IcOpCallV)
        {
            if (rrsrx.Rs)
                rrsrx.Rs = new std::vector<IcRegister>(rrsrx.Rs->begin(), rrsrx.Rs->end());
        }
        else if (Code == IcOpCallI || Code == IcOpCallVI)
        {
            if (crsrx.Rs)
                crsrx.Rs = new std::vector<IcRegister>(crsrx.Rs->begin(), crsrx.Rs->end());
        }
        else if (Code == IcOpPhi)
        {
            if (rsrx.Rs)
                rsrx.Rs = new std::vector<IcRegister>(rsrx.Rs->begin(), rsrx.Rs->end());
        }

        return *this;
    }
};

typedef std::vector<IcOperation> IcOperationList;

enum IcTypeKeyword
{
    IcStructKeyword,
    IcUnionKeyword,
    IcEnumKeyword
};

enum IcTypeKind
{
    IcBaseTypeKind,
    IcPointerTypeKind,
    IcArrayTypeKind,
    IcStructTypeKind,
    IcEnumTypeKind,
    IcIncompleteTypeKind,
    IcFunctionTypeKind
};

struct IcType
{
    IcTypeKind Kind;
    unsigned int Traits;
    union
    {
        struct
        {
            IcBaseType Type;
            std::string *EnumTag;
        } Base;
        struct
        {
            IcType *Child;
        } Pointer;
        struct
        {
            unsigned int Size;
            IcType *Child;
        } Array;
        IcStructType *Struct;
        IcEnumType *Enum;
        struct
        {
            IcTypeKeyword Keyword;
            std::string *Tag;
            int ScopeDepth;
        } Incomplete;
        IcFunctionType *Function;
    };

    bool EqualTo(IcType &other)
    {
        if (Kind != other.Kind)
            return false;

        switch (Kind)
        {
        case IcBaseTypeKind:
            return Base.Type == other.Base.Type;
        case IcPointerTypeKind:
            return Pointer.Child->EqualTo(*other.Pointer.Child);
        case IcArrayTypeKind:
            return Array.Size == other.Array.Size && Array.Child->EqualTo(*other.Array.Child);
        case IcStructTypeKind:
            return EqualToStruct(Struct, other.Struct);
        case IcEnumTypeKind:
            return true;
        case IcIncompleteTypeKind:
            return Incomplete.Keyword == other.Incomplete.Keyword && *Incomplete.Tag == *other.Incomplete.Tag;
        case IcFunctionTypeKind:
            return EqualToFunction(Function, other.Function);
        default:
            throw "invalid type";
        }
    }

    int SizeOf()
    {
        if (Kind == IcBaseTypeKind)
        {
            switch (Base.Type)
            {
            case IcCharType:
                return 1;
            case IcShortType:
                return 2;
            case IcIntType:
                return 4;
            case IcLongType:
                return 4;
            case IcLongLongType:
                return 8;
            case IcFloatType:
                return 4;
            case IcDoubleType:
                return 8;
            case IcVoidType:
                return -1;
            }
        }
        else if (Kind == IcPointerTypeKind)
        {
            return 4;
        }
        else if (Kind == IcArrayTypeKind)
        {
            if (Array.Size == -1)
                return -1;

            return Array.Child->SizeOf() * Array.Size;
        }
        else if (Kind == IcStructTypeKind)
        {
            return SizeOfStruct(Struct);
        }
        else if (Kind == IcEnumTypeKind)
        {
            return 4;
        }
        else if (Kind == IcIncompleteTypeKind)
        {
            return -1;
        }
        else if (Kind == IcFunctionTypeKind)
        {
            return -1;
        }

        return -1;
    }

    int AlignOf()
    {
        if (Kind == IcBaseTypeKind)
        {
            return SizeOf();
        }
        else if (Kind == IcPointerTypeKind)
        {
            return SizeOf();
        }
        else if (Kind == IcArrayTypeKind)
        {
            return Array.Child->AlignOf();
        }
        else if (Kind == IcStructTypeKind)
        {
            return AlignOfStruct(Struct);
        }
        else if (Kind == IcEnumTypeKind)
        {
            return SizeOf();
        }
        else if (Kind == IcIncompleteTypeKind)
        {
            return -1;
        }
        else if (Kind == IcFunctionTypeKind)
        {
            return -1;
        }

        return -1;
    }
};

struct IcStructField
{
    std::string Name;
    IcType Type;
    int Bits;
    int Offset;
};

struct IcStructType
{
    IcTypeKeyword Keyword;
    std::string Tag;
    int UniqueId;
    std::vector<IcStructField> Fields;
    int Size;
    int Align;

    IcStructField *FindField(std::string &name, int *offset = NULL)
    {
        for (auto it = Fields.begin(); it != Fields.end(); ++it)
        {
            if (it->Name.length() == 0)
            {
                if (it->Type.Kind == IcStructTypeKind)
                {
                    int childOffset;

                    // Unnamed struct/union - search inside the field.
                    IcStructField *field = it->Type.Struct->FindField(name, &childOffset);

                    if (field)
                    {
                        if (offset)
                            *offset = it->Offset + childOffset;

                        return field;
                    }
                }
            }
            else if (it->Name == name)
            {
                if (offset)
                    *offset = it->Offset;

                return &*it;
            }
        }

        return NULL;
    }
};

inline bool EqualToStruct(IcStructType *type1, IcStructType *type2)
{
    return type1->UniqueId == type2->UniqueId;
}

inline int SizeOfStruct(IcStructType *type)
{
    return type->Size;
}

inline int AlignOfStruct(IcStructType *type)
{
    return type->Align;
}

struct IcEnumField
{
    std::string Name;
    int Value;
};

struct IcEnumType
{
    std::string Tag;
    std::vector<IcEnumField> Fields;
};

struct IcFunctionType
{
    IcType ReturnType;
    bool Variadic;
    std::vector<IcType> Parameters;
    std::vector<std::string> *ParameterNames;
};

inline bool EqualToFunction(IcFunctionType *type1, IcFunctionType *type2)
{
    if (!type1->ReturnType.EqualTo(type2->ReturnType))
        return false;
    if (type1->Variadic != type2->Variadic)
        return false;
    if (type1->Parameters.size() != type2->Parameters.size())
        return false;

    for (size_t i = 0; i < type1->Parameters.size(); i++)
    {
        if (!type1->Parameters[i].EqualTo(type2->Parameters[i]))
            return false;
    }

    return true;
}

enum IcStorageKind
{
    IcRegisterStorageKind,
    IcIndirectStorageKind,
    IcNoStorageKind
};

struct IcStorage
{
    IcStorageKind Kind;
    union
    {
        struct
        {
            IcRegister Register;
            bool FunctionDesignator;
            int BitCount;
            int BitOffset;
        } r;
        struct
        {
            IcRegister Address;
            bool LValue;
            int BitCount;
            int BitOffset;
        } i;
    };
};

struct IcTypedValue
{
    IcType Type;
    IcStorage Value;
};

typedef int IcLabelId;

extern int IcNextBlockId;

struct IcBlock
{
    static IcLabelId AllocateLabelId()
    {
        return IcNextBlockId++;
    }

    IcOperationList PhiOperations;
    IcOperationList Operations;
    IcLabelId Label;
};

struct IcBlockContext
{
    IcBlockContext *Parent;
    IcBlock *CurrentBlock;
};

enum IcControlKeyword
{
    IcWhileKeyword,
    IcDoWhileKeyword,
    IcForKeyword,
    IcSwitchKeyword
};

struct IcControlScope
{
    IcControlScope *Parent;
    IcControlKeyword Keyword;
    IcBlock *BreakTarget;
    IcBlock *ContinueTarget;
    std::map<long long, IcBlock *> Cases;
    IcBlock *DefaultCase;

    IcControlScope(IcControlKeyword keyword)
        : Parent(NULL), Keyword(keyword), BreakTarget(NULL), ContinueTarget(NULL), DefaultCase(NULL)
    { }
};

struct IcFrameSlot
{
    int Size;
    IcVariableId VariableId;
};

typedef std::multimap<std::string, IcBlock *> IcLabelBlockMap;

struct IcFunctionVariable
{
    bool Parameter;
    int Index;
    IcRegister AddressRegister;
    IcType Type;
};

struct IcFunctionScope
{
    IcBlock *StartBlock;
    IcBlock *EndBlock;
    std::set<IcBlock *> Blocks;
    std::map<IcLabelId, IcBlock *> BlockMap;
    std::map<int, IcFrameSlot> Slots;
    int NextSlotId;
    std::map<IcVariableId, IcFunctionVariable> Variables;
    IcVariableId ReturnVariableId;
    std::map<std::string, IcBlock *> Labels;
    IcLabelBlockMap PendingGotos;
    IcType Type;
    IcOrdinarySymbol *Symbol;
    std::vector<IcOrdinarySymbol *> PendingSymbols;

    IcFunctionScope()
        : StartBlock(NULL), EndBlock(NULL), NextSlotId(1), ReturnVariableId(0)
    { }

    int AllocateSlot(int size, IcVariableId variableId = 0);
    IcBlock *CreateBlock();
};

enum IcSymbolKind
{
    IcVariableSymbolKind,
    IcObjectSymbolKind,
    IcTypedefSymbolKind,
    IcConstantSymbolKind
};

struct IcOrdinarySymbol
{
    IcSymbolKind Kind;
    std::string Name;
    union
    {
        struct
        {
            IcVariableId Id;
        } v;
        struct
        {
            IcType Type;
            IcObjectReference Reference;
        } o;
        struct
        {
            IcType Target;
        } t;
        struct
        {
            int Value;
        } c;
    };
};

struct IcTagSymbol
{
    std::string Name;
    IcType Type;
};

struct IcScope
{
    IcScope *Parent;
    std::map<std::string, IcOrdinarySymbol *> Symbols;
    std::map<std::string, IcTagSymbol *> Tags;

    IcScope()
        : Parent(NULL)
    { }

    IcOrdinarySymbol *LookupSymbol(std::string &name)
    {
        auto it = Symbols.find(name);

        if (it != Symbols.end())
            return it->second;
        if (Parent != NULL)
            return Parent->LookupSymbol(name);

        return NULL;
    }

    IcTagSymbol *LookupTag(std::string &name)
    {
        auto it = Tags.find(name);

        if (it != Tags.end())
            return it->second;
        if (Parent != NULL)
            return Parent->LookupTag(name);

        return NULL;
    }
};
