#pragma once

#include "LrParser.h"
#include "LrGenerator.h"
#include "Lexer.h"
#include "CModel.h"
#include "IntermediateObject.h"
#include "IcEvaluator.h"

struct CParserItem
{
    LrParserItem LrItem;
    std::string Text;
    void *Object;

    IcTypedValue Value; // expression result
};

class CParser : LrParser
{
public:
    ~CParser();
    void Initialize(LrTable *table, LrGenerator *generator, IcObjectManager *objectManager);
    bool Parse(std::istream &stream);
    bool GetError(std::string &errorMessage);
    Token LastToken();

    LrTable *Table();
    LrParserItem Top();
    void Push(LrParserItem item);
    void Pop(int count);
    int ProductionLength(int productionId);
    int ProductionSymbol(int productionId);
    LrAction ProductionConflict(LrTable::const_iterator begin, LrTable::const_iterator end);
    void Shift(int symbol);
    void *BeforeReduce(int productionId);
    void AfterReduce(int productionId, void *context);
    void Accept();

private:
    LrTable *_table;
    LrGenerator *_generator;
    IcObjectManager *_objectManager;
    std::vector<CParserItem> _stack;
    Token _lastToken;

    bool _error;
    std::string _errorMessage;

    IcType _sizeT;
    IcType _intType;
    IcScope *_scope;
    int _scopeDepth;
    IcControlScope *_controlScope;
    IcBlock *_currentBlock;
    IcFunctionScope *_functionScope;
    IcRegister _nextRegister;
    IcVariableId _nextVariableId;
    int _nextStructId;
    IcEvaluator _evaluator;

    bool _disableTypeNames;
    int _disableSideEffects;
    std::vector<CPendingSideEffect> _pendingSideEffects;
    IcTypedValue _value;

    void SetError(std::string &errorMessage);
    void SetError(const char *errorMessage);

    int TokenToSymbolId(Token &token);
    int StringToSymbolId(const char *string);
    void GetStackString(std::string &appendTo);

    IcImmediateValue CreateImmediateNumber(std::string &text, IcType *type = NULL);
    IcImmediateValue CreateImmediateNumber(int value, IcBaseType type);
    IcImmediateValue CreateImmediateChar(std::string &text);
    IcImmediateValue CreateImmediateString(std::string &text, IcType *type);
    void UnescapeString(std::string &output, std::string::const_iterator begin, std::string::const_iterator end);
    void UnescapeStringWide(std::wstring &output, std::string::const_iterator begin, std::string::const_iterator end);
    char ConvertEscapeCharacter(char escape);
    bool ConvertEscapeNumber(std::string::const_iterator &current, std::string::const_iterator end, int &result);

    IcType BasicTypeToIcType(CBasicType basicType);
    IcType CreateIcType(IcType &type, CDeclarator &declarator, bool parameterNames = false);
    IcType CreateIcType(CTypeSpecifier &type, CTypeQualifier qualifier, CDeclarator &declarator, bool typeSpecified, bool parameterNames = false);
    IcRegister AllocateRegister();
    IcVariableId AllocateVariableId();
    void AppendOp(IcOperation &op);
    void PushControlScope(IcControlKeyword keyword);
    void PopControlScope();
    IcScope *GetScopeByDepth(int depth);

    bool SideEffectsDisabled();
    void DisableSideEffects();
    void EnableSideEffects();

    enum CTypeCheckOptions
    {
        CAllowInt = 0x1,
        CAllowFloat = 0x2,
        CAllowPointer = 0x4,
        CAllowArray = 0x8,
        CAllowStruct = 0x10,
        CAllowVoid = 0x20
    };

    bool CheckType(IcType &type, unsigned int options);
    bool CheckCompatiblePointerTypes(IcType &type1, IcType &type2, bool allowVoidPointer);
    IcBaseType GetBaseType(IcType &type);
    IcType GetChildForPointerType(IcType &type);
    IcOpcode OpcodeForSourceType(IcType &type);
    bool IsConstantZero(IcStorage &storage);
    bool IsConstantZero(IcImmediateValue &value);

    enum CTypeConvertOptions
    {
        CDontConvertArray = 0x1 // conversion from array to pointer
    };

    void ConvertOneType(IcTypedValue &value, unsigned int options, IcTypedValue &newValue);
    void CommonNumberType(IcType &type1, IcType &type2, IcType &newType);

    IcRegister ReadValue(IcTypedValue &value, IcType *newType = NULL);
    IcTypedValue LoadInt(int constant, IcBaseType type);

    IcStorage AllocateVariable(IcType &type, IcVariableId *variableId = NULL);
    IcTypedValue GetVariable(IcVariableId variableId);

    void SequencePoint();
    void AssignExpressionAssign(IcTypedValue &input1, IcTypedValue &input2, IcTypedValue &newValue, bool returnValue = false);
    void OrExpression(IcTypedValue &input1, IcTypedValue &input2, IcTypedValue &newValue);
    void XorExpression(IcTypedValue &input1, IcTypedValue &input2, IcTypedValue &newValue);
    void AndExpression(IcTypedValue &input1, IcTypedValue &input2, IcTypedValue &newValue);
    void EqualityExpression(std::string &tag, IcTypedValue &input1, IcTypedValue &input2, IcTypedValue &newValue);
    void RelationalExpression(std::string &tag, IcTypedValue &input1, IcTypedValue &input2, IcTypedValue &newValue);
    void ShiftExpression(std::string &tag, IcTypedValue &input1, IcTypedValue &input2, IcTypedValue &newValue);
    void AddExpressionAdd(IcTypedValue &input1, IcTypedValue &input2, IcTypedValue &newValue, bool pointerModeOnly = false, IcType *pointerType = NULL);
    void AddExpressionSubtract(IcTypedValue &input1, IcTypedValue &input2, IcTypedValue &newValue);
    void MultiplyExpression(std::string &tag, IcTypedValue &input1, IcTypedValue &input2, IcTypedValue &newValue);
    void CastExpression(IcType &type, IcTypedValue &input, IcTypedValue &newValue);
    void CallExpression(IcTypedValue &input, CParameterList *parameters, IcTypedValue &newValue);

    void DefinitionList(CDeclarationList *declarationList);

    void CreateFunctionScope(CDeclaration *declaration, CDeclarator *declarator);
    void DeleteFunctionScope();
};
