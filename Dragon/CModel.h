#pragma once

#include "IntermediateCode.h"

struct CDeclarator;
struct CInitializer;
struct CDeclaration;
struct CStructType;
struct CEnumType;

enum CBasicType
{
    CVoidType = 1,
    CCharType,
    CShortType,
    CIntType,
    CLongType,
    CFloatType,
    CDoubleType,
    CMaximumBasicType,
    CBasicTypeMask = 0xfff,

    CSignedType = 0x1000,
    CUnsignedType = 0x2000
};

enum CStorageClass
{
    CAutoClass = 0x1,
    CExternClass = 0x2,
    CRegisterClass = 0x4,
    CStaticClass = 0x8,
    CTypedefClass = 0x10
};

enum CTypeQualifier
{
    CConstQualifier = 0x1,
    CVolatileQualifier = 0x2
};

#define C_NO_DIMENSION ((unsigned int)-1)

typedef std::vector<CTypeQualifier> CPointerChain;
typedef std::vector<unsigned int> CDimensionList;

struct CDeclaratorParameterList
{
    bool Ellipsis;
    std::vector<CDeclaration> List;

    CDeclaratorParameterList()
        : Ellipsis(false)
    { }
};

struct CDeclarator
{
    std::string Name;
    CDeclarator *Child;
    CPointerChain Chain;
    CDimensionList Dimensions;
    bool Function;
    CDeclaratorParameterList Parameters;

    CDeclarator()
        : Child(NULL), Function(false)
    { }

    CDeclarator(const CDeclarator &other)
    {
        *this = other;
    }

    ~CDeclarator()
    {
        if (Child)
            delete Child;
    }

    CDeclarator &operator=(const CDeclarator &other)
    {
        Name = other.Name;

        if (other.Child)
            Child = new CDeclarator(*other.Child);
        else
            Child = NULL;

        Chain = other.Chain;
        Dimensions = other.Dimensions;
        Function = other.Function;
        Parameters = other.Parameters;

        return *this;
    }

    std::string &FindName()
    {
        if (Child)
        {
            return Child->FindName();
        }

        return Name;
    }
};

enum CTypeSpecifierType
{
    CBasicTypeSpecifier,
    CIcTypeSpecifier
};

struct CTypeSpecifier
{
    CTypeSpecifierType Type;
    union
    {
        CBasicType Basic;
        IcType Ic;
    };
};

typedef std::vector<CInitializer> CInitializerList;

struct CInitializer
{
    bool IsList;
    union
    {
        IcTypedValue *Value;
        CInitializerList *List;
    };

    CInitializer()
        : IsList(false), Value(NULL)
    { }

    CInitializer(const CInitializer &other)
    {
        *this = other;
    }

    ~CInitializer()
    {
        if (IsList)
            delete List;
        else
            delete Value;
    }

    CInitializer &operator=(const CInitializer &other)
    {
        IsList = other.IsList;

        if (other.IsList)
            List = new CInitializerList(*other.List);
        else
            Value = new IcTypedValue(*other.Value);

        return *this;
    }
};

struct CDeclaration
{
    CTypeSpecifier Type;
    CStorageClass Storage;
    CTypeQualifier Qualifiers;
    unsigned int Bits;
    CDeclarator Declarator;
    CInitializer *Initializer;
    bool TypeSpecified;

    CDeclaration()
        : Storage((CStorageClass)0), Qualifiers((CTypeQualifier)0), Bits(0), Initializer(NULL), TypeSpecified(false)
    { }

    CDeclaration(const CDeclaration &other)
    {
        *this = other;
    }

    ~CDeclaration()
    {
        if (Initializer)
            delete Initializer;
    }

    CDeclaration &operator=(const CDeclaration &other)
    {
        Type = other.Type;
        Storage = other.Storage;
        Qualifiers = other.Qualifiers;
        Bits = other.Bits;
        Declarator = other.Declarator;
        TypeSpecified = other.TypeSpecified;

        if (other.Initializer)
            Initializer = new CInitializer(*other.Initializer);
        else
            Initializer = NULL;

        return *this;
    }
};

typedef std::vector<CDeclaration> CDeclarationList;

struct CEnumType
{
    std::string Tag;
    std::map<std::string, unsigned int> Values;
    unsigned int NextValue;

    CEnumType()
        : NextValue(0)
    { }
};

struct CEnumValue
{
    std::string Name;
    bool ValuePresent;
    unsigned int Value;

    CEnumValue()
        : ValuePresent(false), Value(0)
    { }
};

typedef std::vector<CEnumValue> CEnumValueList;

struct CInitializationDeclarator
{
    CDeclarator *Declarator;
    CInitializer *Initializer;
    unsigned int Bits;

    CInitializationDeclarator()
        : Declarator(NULL), Initializer(NULL), Bits(0)
    { }

    CInitializationDeclarator(const CInitializationDeclarator &other)
    {
        *this = other;
    }

    ~CInitializationDeclarator()
    {
        delete Declarator;

        if (Initializer)
            delete Initializer;
    }

    CInitializationDeclarator &operator=(const CInitializationDeclarator &other)
    {
        Declarator = new CDeclarator(*other.Declarator);

        if (other.Initializer)
            Initializer = new CInitializer(*other.Initializer);
        else
            Initializer = NULL;

        Bits = other.Bits;

        return *this;
    }
};

typedef std::vector<CInitializationDeclarator> CInitializationDeclaratorList;

// IC-related support structures

struct CIfStatementContext
{
    IcBlock *StartBlock;
    IcBlock *NextBlock;
    IcBlock *NonZeroBlock;
    IcBlock *ZeroBlock;
};

struct CWhileStatementContext
{
    IcBlock *TestBlock;
    IcBlock *BodyBlock;
    IcBlock *NextBlock;
};

struct CDoWhileStatementContext
{
    IcBlock *BodyBlock;
    IcBlock *NextBlock;
};

struct CForStatementContext
{
    IcBlock *TestBlock;
    IcBlock *BodyBlock;
    IcBlock *PostBlock;
    IcBlock *NextBlock;
};

struct CSwitchStatementContext
{
    IcBlock *StartBlock;
    IcBlock *InnerBlock;
    IcBlock *NextBlock;
};

struct CConditionalExpressionContext
{
    IcBlock *StartBlock;
    IcBlock *NonZeroBlock;
    IcBlock *ZeroBlock;
    IcBlock *NextBlock;
};

struct CLogicalExpressionContext
{
    IcBlock *OtherBlock;
    IcBlock *NextBlock;
    IcRegister AddressRegister;
};

enum CPendingSideEffectType
{
    CPendingIncrementEffect,
    CPendingDecrementEffect
};

struct CPendingSideEffect
{
    CPendingSideEffectType Type;
    IcTypedValue Value;
};

typedef std::vector<IcTypedValue> CParameterList;
