#include "stdafx.h"
#include "CParser.h"

#ifndef NDEBUG
//#define CPARSER_DEBUG_OUTPUT
#endif

CParser::~CParser()
{
    delete _scope;
}

void CParser::Initialize(LrTable *table, LrGenerator *generator, IcObjectManager *objectManager)
{
    LrParserItem item;

    _table = table;
    _generator = generator;
    _objectManager = objectManager;
    memset(&_lastToken, 0, sizeof(Token));

    memset(&_sizeT, 0, sizeof(IcType));
    _sizeT.Kind = IcBaseTypeKind;
    _sizeT.Traits = IcUnsignedTrait;
    _sizeT.Base.Type = IcIntType;
    memset(&_intType, 0, sizeof(IcType));
    _intType.Kind = IcBaseTypeKind;
    _intType.Base.Type = IcIntType;

    _scope = new IcScope;
    _scopeDepth = 0;
    _controlScope = NULL;
    _currentBlock = NULL;
    _functionScope = NULL;
    _nextRegister = 1;
    _nextVariableId = 1;
    _nextStructId = 1;
    _disableTypeNames = false;
    _disableSideEffects = 0;

    item.State = 0;
    item.Symbol = _generator->GetEofSymbol()->Id;
    item.Context = NULL;
    Push(item);
}

bool CParser::Parse(std::istream &stream)
{
    Lexer lexer(stream);
    Token token;

    lexer.SetOptions(LEXER_ALLOW_LINE_COMMENTS | LEXER_ALLOW_DOLLAR_SIGN_IN_ID);

    while (lexer.GetToken(token))
    {
        int symbolId = TokenToSymbolId(token);

        if (symbolId == -1)
            continue;

        _lastToken = token;

        if (Consume(this, symbolId, &token) == LrError)
            return false;
    }

    if (Consume(this, _generator->GetEofSymbol()->Id, NULL) != LrAccept)
        return false;

    return true;
}

Token CParser::LastToken()
{
    return _lastToken;
}

LrTable *CParser::Table()
{
    return _table;
}

LrParserItem CParser::Top()
{
    return _stack.back().LrItem;
}

void CParser::Push(LrParserItem item)
{
    CParserItem cItem;

    memset(&cItem, 0, sizeof(CParserItem));
    cItem.LrItem = item;
    _stack.push_back(cItem);
}

void CParser::Pop(int count)
{
    _stack.erase(_stack.end() - count, _stack.end());
}

int CParser::ProductionLength(int productionId)
{
    return _generator->GetProduction(productionId)->Rhs.size();
}

int CParser::ProductionSymbol(int productionId)
{
    return _generator->GetProduction(productionId)->Lhs->Id;
}

LrAction CParser::ProductionConflict(LrTable::const_iterator begin, LrTable::const_iterator end)
{
    LrTable::const_iterator begin2 = begin;

    begin2++;

    if (begin->second.Type == LrReduce && _generator->GetProduction(begin->second.Id)->Lhs->Text == "Type" ||
        (begin2 != end && begin2->second.Type == LrReduce && _generator->GetProduction(begin2->second.Id)->Lhs->Text == "Type"))
    {
        LrAction typeAction;
        LrAction otherAction;

        if (_generator->GetProduction(begin->second.Id)->Lhs->Text == "Type")
        {
            typeAction = begin->second;
            otherAction = begin2->second;
        }
        else
        {
            typeAction = begin2->second;
            otherAction = begin->second;
        }

        if (_disableTypeNames)
            return otherAction;

        IcOrdinarySymbol *ordinarySymbol = _scope->LookupSymbol(_stack[_stack.size() - 1].Text);

        if (ordinarySymbol && ordinarySymbol->Kind == IcTypedefSymbolKind)
            return typeAction;

        return otherAction;
    }

    throw "unrecognized production conflict";
}

void CParser::Shift(int symbol)
{
#ifdef CPARSER_DEBUG_OUTPUT
    std::cerr << "shift " << _generator->GetSymbol(symbol)->Text << std::endl;
    std::string s;
    GetStackString(s);
    std::cerr << s << std::endl;
#endif

    _stack[_stack.size() - 1].Text = ((Token *)_stack[_stack.size() - 1].LrItem.Context)->Text;
}

void *CParser::BeforeReduce(int productionId)
{
    std::string &name = _generator->GetProduction(productionId)->Lhs->Text;
    std::string &tag = _generator->GetProduction(productionId)->Tag;
    int stackIndex;

    stackIndex = _stack.size() - ProductionLength(productionId);
    memset(&_value, 0, sizeof(IcTypedValue));

    if (false)
    {
        // Nothing
    }
    else if (name == "$EnableTypeName")
    {
        _disableTypeNames = false;
    }
    else if (name == "$CreateFunctionScope")
    {
        CDeclaration *declaration = (CDeclaration *)_stack[stackIndex - 3].Object;
        CDeclarator *declarator = (CDeclarator *)_stack[stackIndex - 2].Object;

        CreateFunctionScope(declaration, declarator);
    }
    else if (name == "$DeleteFunctionScope")
    {
        DeleteFunctionScope();
    }
    else if (name == "$PushScope")
    {
        IcScope *child = new IcScope;

        child->Parent = _scope;
        _scope = child;
        _scopeDepth++;

        if (_functionScope->PendingSymbols.size() != 0)
        {
            for (auto it = _functionScope->PendingSymbols.begin(); it != _functionScope->PendingSymbols.end(); ++it)
            {
                if (_scope->Symbols.find((*it)->Name) != _scope->Symbols.end())
                    throw "parameter name already in use";

                _scope->Symbols[(*it)->Name] = *it;
            }

            _functionScope->PendingSymbols.clear();
        }
    }
    else if (name == "$PopScope")
    {
        IcScope *child = _scope;

        _scope = _scope->Parent;
        delete child;
        _scopeDepth--;
    }
    else if (name == "$IdLabel")
    {
        std::string &name = _stack[stackIndex - 2].Text;
        IcOperation op;

        if (_functionScope->Labels.find(name) != _functionScope->Labels.end())
            throw "label already defined";

        IcBlock *newBlock = _functionScope->CreateBlock();

        _functionScope->Labels[name] = newBlock;

        op.Code = IcOpJmp;
        op.b.B1 = newBlock;
        AppendOp(op);

        _currentBlock = newBlock;

        auto range = _functionScope->PendingGotos.equal_range(name);

        for (auto it = range.first; it != range.second; ++it)
        {
            assert(it->second->Operations.back().Code == IcOpJmp);
            it->second->Operations.back().b.B1 = newBlock;
        }

        _functionScope->PendingGotos.erase(range.first, range.second);
    }
    else if (name == "$CaseLabel")
    {
        IcTypedValue value;
        IcControlScope *controlScope = _controlScope;
        IcImmediateValue constantValue;
        IcOperation op;

        while (controlScope && controlScope->Keyword != IcSwitchKeyword)
            controlScope = controlScope->Parent;

        if (!controlScope)
            throw "illegal 'case' label";

        ConvertOneType(_stack[stackIndex - 2].Value, 0, value);

        if (!CheckType(value.Type, CAllowInt) || !_evaluator.GetRegister(ReadValue(value, &_intType), constantValue))
            throw "'case' value must be a constant integer expression";
        if (controlScope->Cases.find((unsigned int)constantValue.u.Int) != controlScope->Cases.end())
            throw "'case' value already defined";

        IcBlock *newBlock = _functionScope->CreateBlock();

        controlScope->Cases[(unsigned int)constantValue.u.Int] = newBlock;

        op.Code = IcOpJmp;
        op.b.B1 = newBlock;
        AppendOp(op);

        _currentBlock = newBlock;
    }
    else if (name == "$DefaultLabel")
    {
        IcControlScope *controlScope = _controlScope;
        IcOperation op;

        while (controlScope && controlScope->Keyword != IcSwitchKeyword)
            controlScope = controlScope->Parent;

        if (!controlScope)
            throw "illegal 'default' label";
        if (controlScope->DefaultCase)
            throw "'default' case already defined";

        IcBlock *newBlock = _functionScope->CreateBlock();

        controlScope->DefaultCase = newBlock;

        op.Code = IcOpJmp;
        op.b.B1 = newBlock;
        AppendOp(op);

        _currentBlock = newBlock;
    }
    else if (name == "$If1")
    {
        IcTypedValue value;
        IcOperation op;

        // Beginning of If or IfElse statement.

        ConvertOneType(_stack[stackIndex - 2].Value, 0, value);

        if (!CheckType(value.Type, CAllowInt | CAllowFloat | CAllowPointer))
            throw "invalid type for 'if' statement";

        SequencePoint();

        CIfStatementContext *context = new CIfStatementContext;

        _stack[stackIndex - 4].Object = context;
        context->StartBlock = _currentBlock;
        context->NextBlock = _functionScope->CreateBlock();
        context->NonZeroBlock = _functionScope->CreateBlock();
        context->ZeroBlock = NULL;

        op.Code = IcOpBrEI;
        op.Mode = GetBaseType(value.Type);
        op.rcbb.R1 = ReadValue(value);
        op.rcbb.C2 = CreateImmediateNumber(0, op.Mode);
        op.rcbb.B3 = context->NextBlock;
        op.rcbb.B4 = context->NonZeroBlock;
        AppendOp(op);

        _currentBlock = context->NonZeroBlock;
    }
    else if (name == "$If2")
    {
        CIfStatementContext *context = (CIfStatementContext *)_stack[stackIndex - 6].Object;
        IcOperation op;

        // End of If statement.

        op.Code = IcOpJmp;
        op.b.B1 = context->NextBlock;
        AppendOp(op);

        _currentBlock = context->NextBlock;
    }
    else if (name == "$IfElse1")
    {
        CIfStatementContext *context = (CIfStatementContext *)_stack[stackIndex - 7].Object;
        IcOperation op;

        // This is an IfElse statement, not an If statement. Modify our initial assumptions.

        context->ZeroBlock = _functionScope->CreateBlock();

        assert(context->StartBlock->Operations.back().Code == IcOpBrEI);
        context->StartBlock->Operations.back().rcbb.B3 = context->ZeroBlock;

        op.Code = IcOpJmp;
        op.b.B1 = context->NextBlock;
        AppendOp(op);

        _currentBlock = context->ZeroBlock;
    }
    else if (name == "$IfElse2")
    {
        CIfStatementContext *context = (CIfStatementContext *)_stack[stackIndex - 9].Object;
        IcOperation op;

        // End of IfElse statement.

        op.Code = IcOpJmp;
        op.b.B1 = context->NextBlock;
        AppendOp(op);

        _currentBlock = context->NextBlock;
    }
    else if (name == "$While1")
    {
        CWhileStatementContext *context = new CWhileStatementContext;
        IcOperation op;

        _stack[stackIndex - 1].Object = context;
        context->TestBlock = _functionScope->CreateBlock();
        context->BodyBlock = NULL;
        context->NextBlock = _functionScope->CreateBlock();

        PushControlScope(IcWhileKeyword);
        _controlScope->BreakTarget = context->NextBlock;
        _controlScope->ContinueTarget = context->TestBlock;

        op.Code = IcOpJmp;
        op.b.B1 = context->TestBlock;
        AppendOp(op);

        _currentBlock = context->TestBlock;
    }
    else if (name == "$While2")
    {
        IcTypedValue value;
        CWhileStatementContext *context = (CWhileStatementContext *)_stack[stackIndex - 5].Object;
        IcOperation op;

        ConvertOneType(_stack[stackIndex - 2].Value, 0, value);

        if (!CheckType(value.Type, CAllowInt | CAllowFloat | CAllowPointer))
            throw "invalid type for 'while' statement";

        SequencePoint();

        context->BodyBlock = _functionScope->CreateBlock();

        op.Code = IcOpBrEI;
        op.Mode = GetBaseType(value.Type);
        op.rcbb.R1 = ReadValue(value);
        op.rcbb.C2 = CreateImmediateNumber(0, op.Mode);
        op.rcbb.B3 = context->NextBlock;
        op.rcbb.B4 = context->BodyBlock;
        AppendOp(op);

        _currentBlock = context->BodyBlock;
    }
    else if (name == "$While3")
    {
        CWhileStatementContext *context = (CWhileStatementContext *)_stack[stackIndex - 7].Object;
        IcOperation op;

        op.Code = IcOpJmp;
        op.b.B1 = context->TestBlock;
        AppendOp(op);

        PopControlScope();

        _currentBlock = context->NextBlock;
    }
    else if (name == "$DoWhile1")
    {
        CDoWhileStatementContext *context = new CDoWhileStatementContext;
        IcOperation op;

        _stack[stackIndex - 1].Object = context;
        context->BodyBlock = _functionScope->CreateBlock();
        context->NextBlock = _functionScope->CreateBlock();

        PushControlScope(IcDoWhileKeyword);
        _controlScope->BreakTarget = context->NextBlock;
        _controlScope->ContinueTarget = context->BodyBlock;

        op.Code = IcOpJmp;
        op.b.B1 = context->BodyBlock;
        AppendOp(op);

        _currentBlock = context->BodyBlock;
    }
    else if (name == "$DoWhile2")
    {
        IcTypedValue value;
        CDoWhileStatementContext *context = (CDoWhileStatementContext *)_stack[stackIndex - 7].Object;
        IcOperation op;

        ConvertOneType(_stack[stackIndex - 2].Value, 0, value);

        if (!CheckType(value.Type, CAllowInt | CAllowFloat | CAllowPointer))
            throw "invalid type for 'do-while' statement";

        SequencePoint();

        op.Code = IcOpBrEI;
        op.Mode = GetBaseType(value.Type);
        op.rcbb.R1 = ReadValue(value);
        op.rcbb.C2 = CreateImmediateNumber(0, op.Mode);
        op.rcbb.B3 = context->NextBlock;
        op.rcbb.B4 = context->BodyBlock;
        AppendOp(op);

        PopControlScope();

        _currentBlock = context->NextBlock;
    }
    else if (name == "$For1")
    {
        CForStatementContext *context = new CForStatementContext;
        IcOperation op;

        SequencePoint();

        _stack[stackIndex - 3].Object = context;
        context->TestBlock = _functionScope->CreateBlock();
        context->BodyBlock = NULL;
        context->PostBlock = NULL;
        context->NextBlock = _functionScope->CreateBlock();

        PushControlScope(IcForKeyword);
        _controlScope->BreakTarget = context->NextBlock;
        _controlScope->ContinueTarget = context->TestBlock;

        op.Code = IcOpJmp;
        op.b.B1 = context->TestBlock;
        AppendOp(op);

        _currentBlock = context->TestBlock;
    }
    else if (name == "$For2A" || name == "$For2B")
    {
        IcTypedValue value;
        CForStatementContext *context = (CForStatementContext *)_stack[stackIndex - 5].Object;
        IcOperation op;

        ConvertOneType(_stack[stackIndex - 1].Value, 0, value);

        if (!CheckType(value.Type, CAllowInt | CAllowFloat | CAllowPointer))
            throw "invalid type for 'for' condition statement";

        SequencePoint();

        context->BodyBlock = _functionScope->CreateBlock();

        op.Code = IcOpBrEI;
        op.Mode = GetBaseType(value.Type);
        op.rcbb.R1 = ReadValue(value);
        op.rcbb.C2 = CreateImmediateNumber(0, op.Mode);
        op.rcbb.B3 = context->NextBlock;
        op.rcbb.B4 = context->BodyBlock;
        AppendOp(op);

        if (name == "$For2B")
        {
            context->PostBlock = _functionScope->CreateBlock();
            _currentBlock = context->PostBlock;
        }
    }
    else if (name == "$For3A" || name == "$For3B")
    {
        CForStatementContext *context = (CForStatementContext *)_stack[stackIndex - (name == "$For3A" ? 7 : 8)].Object;

        if (name == "$For3B")
            SequencePoint();

        _currentBlock = context->BodyBlock;
    }
    else if (name == "$For4A" || name == "$For4B")
    {
        CForStatementContext *context = (CForStatementContext *)_stack[stackIndex - (name == "$For4A" ? 9 : 10)].Object;
        IcOperation op;

        if (name == "$For4B")
        {
            op.Code = IcOpJmp;
            op.b.B1 = context->PostBlock;
            AppendOp(op);

            _currentBlock = context->PostBlock;
        }

        op.Code = IcOpJmp;
        op.b.B1 = context->TestBlock;
        AppendOp(op);

        PopControlScope();

        _currentBlock = context->NextBlock;
    }
    else if (name == "$Switch1")
    {
        CSwitchStatementContext *context = new CSwitchStatementContext;

        SequencePoint();

        _stack[stackIndex - 4].Object = context;
        context->StartBlock = _currentBlock;
        context->InnerBlock = _functionScope->CreateBlock();
        context->NextBlock = _functionScope->CreateBlock();

        PushControlScope(IcSwitchKeyword);
        _controlScope->BreakTarget = context->NextBlock;

        _currentBlock = context->InnerBlock;
    }
    else if (name == "$Switch2")
    {
        IcTypedValue value;
        CSwitchStatementContext *context = (CSwitchStatementContext *)_stack[stackIndex - 6].Object;
        IcRegister valueRegister;
        IcOperation op;

        op.Code = IcOpJmp;
        op.b.B1 = context->NextBlock;
        AppendOp(op);

        ConvertOneType(_stack[stackIndex - 4].Value, 0, value);

        if (!CheckType(value.Type, CAllowInt))
            throw "invalid type for 'switch' statement";

        valueRegister = ReadValue(value);

        _currentBlock = context->StartBlock;

        if (_controlScope->Cases.size() != 0)
        {
            IcBlock *previousBlock = NULL;

            for (auto it = _controlScope->Cases.begin(); it != _controlScope->Cases.end(); ++it)
            {
                if (previousBlock)
                {
                    assert(previousBlock->Operations.back().Code == IcOpBrEI);
                    _currentBlock = _functionScope->CreateBlock();
                    previousBlock->Operations.back().rcbb.B4 = _currentBlock;
                }

                op.Code = IcOpBrEI;
                op.Mode = IcIntType;
                op.rcbb.R1 = valueRegister;
                op.rcbb.C2 = CreateImmediateNumber((int)it->first, IcIntType);
                op.rcbb.B3 = it->second;
                op.rcbb.B4 = NULL; // filled in later
                AppendOp(op);

                previousBlock = _currentBlock;
            }

            assert(previousBlock);
            assert(previousBlock->Operations.back().Code == IcOpBrEI);

            if (_controlScope->DefaultCase)
            {
                previousBlock->Operations.back().rcbb.B4 = _controlScope->DefaultCase;
            }
            else
            {
                previousBlock->Operations.back().rcbb.B4 = context->NextBlock;
            }
        }
        else
        {
            if (_controlScope->DefaultCase)
            {
                op.Code = IcOpJmp;
                op.b.B1 = _controlScope->DefaultCase;
                AppendOp(op);
            }
            else
            {
                op.Code = IcOpJmp;
                op.b.B1 = context->NextBlock;
                AppendOp(op);
            }
        }

        PopControlScope();

        _currentBlock = context->NextBlock;
    }
    else if (name == "$Comma1")
    {
        SequencePoint();
    }
    else if (name == "$Conditional1")
    {
        IcTypedValue value;

        ConvertOneType(_stack[stackIndex - 2].Value, 0, value);

        if (!CheckType(value.Type, CAllowInt | CAllowFloat | CAllowPointer))
            throw "invalid type for ternary conditional operator";

        SequencePoint();

        if (SideEffectsDisabled())
            return NULL;

        CConditionalExpressionContext *context = new CConditionalExpressionContext;
        IcOperation op;

        _stack[stackIndex - 1].Object = context;
        context->StartBlock = _currentBlock;
        context->NonZeroBlock = _functionScope->CreateBlock();
        context->ZeroBlock = _functionScope->CreateBlock();
        context->NextBlock = _functionScope->CreateBlock();

        op.Code = IcOpBrEI;
        op.Mode = GetBaseType(value.Type);
        op.rcbb.R1 = ReadValue(value);
        op.rcbb.C2 = CreateImmediateNumber(0, op.Mode);
        op.rcbb.B3 = context->ZeroBlock;
        op.rcbb.B4 = context->NonZeroBlock;
        AppendOp(op);

        _currentBlock = context->NonZeroBlock;
    }
    else if (name == "$Conditional2")
    {
        if (SideEffectsDisabled())
            return NULL;

        CConditionalExpressionContext *context = (CConditionalExpressionContext *)_stack[stackIndex - 4].Object;

        _currentBlock = context->ZeroBlock;
    }
    else if (name == "$AndAnd1" || name == "$OrOr1")
    {
        IcTypedValue value;

        ConvertOneType(_stack[stackIndex - 2].Value, 0, value);

        if (!CheckType(value.Type, CAllowInt | CAllowFloat | CAllowPointer))
            throw name == "$OrOr1" ? "invalid type for '||' operator" : "invalid type for '&&' operator";

        SequencePoint();

        if (SideEffectsDisabled())
            return NULL;

        CLogicalExpressionContext *context = new CLogicalExpressionContext;
        IcOperation op;

        _stack[stackIndex - 1].Object = context;
        context->NextBlock = _functionScope->CreateBlock();

        context->AddressRegister = AllocateRegister();
        op.Code = IcOpLdF;
        op.Mode = IcIntPtrType;
        op.cr.C1 = CreateImmediateNumber(_functionScope->AllocateSlot(_intType.SizeOf()), IcIntType);
        op.cr.R2 = context->AddressRegister;
        AppendOp(op);

        op.Code = IcOpStI;
        op.Mode = IcIntType;
        op.cr.C1 = CreateImmediateNumber(name == "$OrOr1" ? 1 : 0, IcIntType);
        op.cr.R2 = context->AddressRegister;
        AppendOp(op);

        context->OtherBlock = _functionScope->CreateBlock();

        op.Code = IcOpBrEI;
        op.Mode = GetBaseType(value.Type);
        op.rcbb.R1 = ReadValue(value);
        op.rcbb.C2 = CreateImmediateNumber(0, op.Mode);

        if (name == "$OrOr1")
        {
            op.rcbb.B3 = context->OtherBlock;
            op.rcbb.B4 = context->NextBlock;
        }
        else
        {
            op.rcbb.B3 = context->NextBlock;
            op.rcbb.B4 = context->OtherBlock;
        }

        AppendOp(op);

        _currentBlock = context->OtherBlock;
    }
    else if (name == "$AndAnd2" || name == "$OrOr2")
    {
        IcTypedValue value;

        ConvertOneType(_stack[stackIndex - 1].Value, 0, value);

        if (!CheckType(value.Type, CAllowInt | CAllowFloat | CAllowPointer))
            throw name == "$OrOr2" ? "invalid type for '||' operator" : "invalid type for '&&' operator";

        if (SideEffectsDisabled())
            return NULL;

        CLogicalExpressionContext *context = (CLogicalExpressionContext *)_stack[stackIndex - 3].Object;
        IcOperation op;

        op.Code = IcOpChEI;
        op.Mode = GetBaseType(value.Type);
        op.rcccr.R1 = ReadValue(value);
        op.rcccr.C2 = CreateImmediateNumber(0, op.Mode);
        op.rcccr.C3 = CreateImmediateNumber(0, IcIntType);
        op.rcccr.C4 = CreateImmediateNumber(1, IcIntType);
        op.rcccr.R5 = AllocateRegister();
        AppendOp(op);

        op.Code = IcOpSt;
        op.Mode = IcIntType;
        op.rr.R1 = op.rcccr.R5;
        op.rr.R2 = context->AddressRegister;

        op.Code = IcOpJmp;
        op.b.B1 = context->NextBlock;
        AppendOp(op);

        _currentBlock = context->NextBlock;
    }
    else if (name == "$SizeOf1")
    {
        DisableSideEffects();
    }
    else if (name == "$SizeOf2")
    {
        EnableSideEffects();
    }
    else if (name == "BasicType")
    {
        CBasicType type = (CBasicType)0;
        std::string &text = _stack[stackIndex].Text;

        if (text == "void")
            type = CVoidType;
        else if (text == "char")
            type = CCharType;
        else if (text == "short")
            type = CShortType;
        else if (text == "int")
            type = CIntType;
        else if (text == "long")
            type = CLongType;
        else if (text == "float")
            type = CFloatType;
        else if (text == "double")
            type = CDoubleType;
        else if (text == "signed")
            type = CSignedType;
        else if (text == "unsigned")
            type = CUnsignedType;

        return (void *)type;
    }
    else if (name == "StorageClass")
    {
        CStorageClass storage = (CStorageClass)0;
        std::string &text = _stack[stackIndex].Text;

        if (text == "auto")
            storage = CAutoClass;
        else if (text == "extern")
            storage = CExternClass;
        else if (text == "register")
            storage = CRegisterClass;
        else if (text == "static")
            storage = CStaticClass;
        else if (text == "typedef")
            storage = CTypedefClass;

        return (void *)storage;
    }
    else if (name == "TypeQualifier")
    {
        CTypeQualifier qualifier = (CTypeQualifier)0;
        std::string &text = _stack[stackIndex].Text;

        if (text == "const")
            qualifier = CConstQualifier;
        else if (text == "volatile")
            qualifier = CVolatileQualifier;

        return (void *)qualifier;
    }
    else if (name == "TypeQualifierList")
    {
        unsigned int qualifier;

        if (tag == "One")
        {
            qualifier = (unsigned int)_stack[stackIndex].Object;
        }
        else if (tag == "Many")
        {
            unsigned int newQualifier;

            qualifier = (unsigned int)_stack[stackIndex].Object;
            newQualifier = (unsigned int)_stack[stackIndex + 1].Object;

            // TODO: Check for same qualifier being used twice.
            qualifier |= newQualifier;
        }

        return (void *)qualifier;
    }
    else if (name == "Declarator")
    {
        CDeclarator *declarator;

        if (tag == "Pointer")
        {
            CPointerChain *chain = (CPointerChain *)_stack[stackIndex].Object;

            declarator = (CDeclarator *)_stack[stackIndex + 1].Object;
            declarator->Chain = *chain;
            delete chain;
        }
        else if (tag == "Direct")
        {
            declarator = (CDeclarator *)_stack[stackIndex].Object;
        }

        return declarator;
    }
    else if (name == "DirectDeclarator")
    {
        CDeclarator *declarator;

        if (tag == "Id")
        {
            declarator = new CDeclarator;
            declarator->Name = _stack[stackIndex].Text;
        }
        else if (tag == "Group")
        {
            declarator = new CDeclarator;
            declarator->Child = (CDeclarator *)_stack[stackIndex + 1].Object;
        }
        else if (tag == "Array")
        {
            declarator = (CDeclarator *)_stack[stackIndex].Object;
            declarator->Dimensions.push_back(C_NO_DIMENSION);
        }
        else if (tag == "Array2")
        {
            IcTypedValue convertedValue;
            IcImmediateValue value;

            ConvertOneType(_stack[stackIndex + 2].Value, 0, convertedValue);

            if (!CheckType(convertedValue.Type, CAllowInt) || !_evaluator.GetRegister(ReadValue(convertedValue, &_sizeT), value))
                throw "array dimension must be a constant integer expression";

            declarator = (CDeclarator *)_stack[stackIndex].Object;
            declarator->Dimensions.push_back(value.u.Int);
        }
        else if (tag == "Function")
        {
            declarator = (CDeclarator *)_stack[stackIndex].Object;
            declarator->Function = true;
        }
        else if (tag == "Function2")
        {
            CDeclaratorParameterList *parameterList = (CDeclaratorParameterList *)_stack[stackIndex + 2].Object;

            declarator = (CDeclarator *)_stack[stackIndex].Object;
            declarator->Function = true;
            declarator->Parameters = *parameterList;
            delete parameterList;
        }

        return declarator;
    }
    else if (name == "Pointer")
    {
        CPointerChain *chain;

        if (tag == "One")
        {
            chain = new CPointerChain;
            chain->push_back((CTypeQualifier)0);
        }
        else if (tag == "One2")
        {
            chain = new CPointerChain;
            chain->push_back((CTypeQualifier)(unsigned int)_stack[stackIndex + 1].Object);
        }
        else if (tag == "Many")
        {
            chain = (CPointerChain *)_stack[stackIndex].Object;
            chain->push_back((CTypeQualifier)0);
        }
        else if (tag == "Many2")
        {
            chain = (CPointerChain *)_stack[stackIndex].Object;
            chain->push_back((CTypeQualifier)(unsigned int)_stack[stackIndex + 2].Object);
        }

        return chain;
    }
    else if (name == "AbstractDeclarator")
    {
        CDeclarator *declarator;

        if (tag == "Pointer")
        {
            CPointerChain *chain = (CPointerChain *)_stack[stackIndex].Object;

            declarator = new CDeclarator;
            declarator->Chain = *chain;
            delete chain;
        }
        else if (tag == "Direct")
        {
            declarator = (CDeclarator *)_stack[stackIndex].Object;
        }
        else if (tag == "Both")
        {
            CPointerChain *chain = (CPointerChain *)_stack[stackIndex].Object;

            declarator = (CDeclarator *)_stack[stackIndex + 1].Object;
            declarator->Chain = *chain;
            delete chain;
        }

        return declarator;
    }
    else if (name == "DirectAbstractDeclarator")
    {
        CDeclarator *declarator;

        if (tag == "Group")
        {
            declarator = new CDeclarator;
            declarator->Child = (CDeclarator *)_stack[stackIndex + 1].Object;
        }
        else if (tag == "OneArray")
        {
            declarator = new CDeclarator;
            declarator->Dimensions.push_back(C_NO_DIMENSION);
        }
        else if (tag == "OneArray2")
        {
            IcTypedValue convertedValue;
            IcImmediateValue value;

            ConvertOneType(_stack[stackIndex + 1].Value, 0, convertedValue);

            if (!CheckType(convertedValue.Type, CAllowInt) || !_evaluator.GetRegister(ReadValue(convertedValue, &_sizeT), value))
                throw "array dimension must be a constant integer expression";

            declarator = new CDeclarator;
            declarator->Dimensions.push_back(value.u.Int);
        }
        else if (tag == "OneFunction")
        {
            declarator = new CDeclarator;
            declarator->Function = true;
        }
        else if (tag == "OneFunction2")
        {
            CDeclaratorParameterList *parameterList = (CDeclaratorParameterList *)_stack[stackIndex + 1].Object;

            declarator = new CDeclarator;
            declarator->Function = true;
            declarator->Parameters = *parameterList;
            delete parameterList;
        }
        else if (tag == "ManyArray")
        {
            declarator = (CDeclarator *)_stack[stackIndex].Object;
            declarator->Dimensions.push_back(C_NO_DIMENSION);
        }
        else if (tag == "ManyArray2")
        {
            IcTypedValue convertedValue;
            IcImmediateValue value;

            ConvertOneType(_stack[stackIndex + 2].Value, 0, convertedValue);

            if (!CheckType(convertedValue.Type, CAllowInt) || !_evaluator.GetRegister(ReadValue(convertedValue, &_sizeT), value))
                throw "array dimension must be a constant integer expression";

            declarator = (CDeclarator *)_stack[stackIndex].Object;
            declarator->Dimensions.push_back(value.u.Int);
        }
        else if (tag == "ManyFunction")
        {
            declarator = (CDeclarator *)_stack[stackIndex].Object;
            declarator->Function = true;
        }
        else if (tag == "ManyFunction2")
        {
            CDeclaratorParameterList *parameterList = (CDeclaratorParameterList *)_stack[stackIndex + 2].Object;

            declarator = (CDeclarator *)_stack[stackIndex].Object;
            declarator->Function = true;
            declarator->Parameters = *parameterList;
            delete parameterList;
        }

        return declarator;
    }
    else if (name == "Type")
    {
        CTypeSpecifier *specifier;

        if (tag == "Name")
        {
            IcOrdinarySymbol *symbol = _scope->LookupSymbol(_stack[stackIndex].Text);

            if (!symbol)
            {
                throw "undefined symbol";
            }
            else if (symbol->Kind == IcTypedefSymbolKind)
            {
                specifier = new CTypeSpecifier;
                specifier->Type = CIcTypeSpecifier;
                specifier->Ic = symbol->t.Target;
            }
            else
            {
                throw "Not a typedef symbol";
            }

            _disableTypeNames = true;
        }
        else if (tag == "Basic")
        {
            specifier = new CTypeSpecifier;
            specifier->Type = CBasicTypeSpecifier;
            specifier->Basic = (CBasicType)(unsigned int)_stack[stackIndex].Object;
        }
        else if (tag == "Aggregate")
        {
            specifier = (CTypeSpecifier *)_stack[stackIndex].Object;
        }
        else if (tag == "Enum")
        {
            specifier = (CTypeSpecifier *)_stack[stackIndex].Object;
        }

        return specifier;
    }
    else if (name == "StructOrUnion")
    {
        std::string &text = _stack[stackIndex].Text;

        if (text == "struct")
            return (void *)false;
        else
            return (void *)true;
    }
    else if (name == "StructOrUnionType")
    {
        CTypeSpecifier *specifier;
        IcTypeKeyword keyword = _stack[stackIndex].Object ? IcUnionKeyword : IcStructKeyword;

        if (tag == "Name")
        {
            IcTagSymbol *tagSymbol = _scope->LookupTag(_stack[stackIndex + 1].Text);

            specifier = new CTypeSpecifier;
            specifier->Type = CIcTypeSpecifier;

            if (tagSymbol)
            {
                if (tagSymbol->Type.Kind != IcStructTypeKind)
                    throw "tag type mismatch";
                if (tagSymbol->Type.Struct->Keyword != keyword)
                    throw "tag keyword mismatch";

                specifier->Ic = tagSymbol->Type;
            }
            else
            {
                specifier->Ic.Kind = IcIncompleteTypeKind;
                specifier->Ic.Traits = 0;
                specifier->Ic.Incomplete.Keyword = keyword;
                specifier->Ic.Incomplete.Tag = new std::string(_stack[stackIndex + 1].Text);
                specifier->Ic.Incomplete.ScopeDepth = _scopeDepth;
            }
        }
        else if (tag == "Fields" || tag == "NameFields")
        {
            IcStructType *structType;
            CDeclarationList *declarationList;

            specifier = new CTypeSpecifier;
            specifier->Type = CIcTypeSpecifier;
            specifier->Ic.Kind = IcStructTypeKind;
            specifier->Ic.Traits = 0;
            structType = new IcStructType;
            specifier->Ic.Struct = structType;

            if (tag == "Fields")
            {
                declarationList = (CDeclarationList *)_stack[stackIndex + 2].Object;
            }
            else
            {
                structType->Tag = _stack[stackIndex + 1].Text;
                declarationList = (CDeclarationList *)_stack[stackIndex + 3].Object;
            }

            structType->Keyword = keyword;
            structType->UniqueId = _nextStructId++;

            int currentOffset = 0;
            int currentAlign = 1;
            int maxSize = 0;

            for (auto it = declarationList->begin(); it != declarationList->end(); ++it)
            {
                IcStructField field;

                field.Type = CreateIcType(it->Type, it->Qualifiers, it->Declarator, it->TypeSpecified);

                if (!CheckType(field.Type, CAllowInt | CAllowFloat | CAllowPointer | CAllowArray | CAllowStruct))
                    throw "invalid type for struct field";

                int sizeOfField = field.Type.SizeOf();
                int alignOfField = field.Type.AlignOf();

                if (sizeOfField == -1 || alignOfField == -1)
                    throw "cannot calculate size of struct field";

                currentOffset = (currentOffset + alignOfField - 1) & ~(alignOfField - 1);

                field.Name = it->Declarator.FindName();
                field.Bits = it->Bits;
                field.Offset = currentOffset;
                structType->Fields.push_back(field);

                if (keyword == IcStructKeyword)
                {
                    currentOffset += sizeOfField;
                }
                else
                {
                    if (maxSize < sizeOfField)
                        maxSize = sizeOfField;
                }

                if (currentAlign < alignOfField)
                    currentAlign = alignOfField;
            }

            if (keyword == IcStructKeyword)
                structType->Size = (currentOffset + currentAlign - 1) & ~(currentAlign - 1);
            else
                structType->Size = maxSize;

            if (structType->Size == 0)
                throw "struct must have at least one member";

            structType->Align = currentAlign;

            if (structType->Tag.length() != 0)
            {
                if (_scope->Tags.find(structType->Tag) != _scope->Tags.end())
                    throw "tag already in use";

                IcTagSymbol *tagSymbol = new IcTagSymbol;

                tagSymbol->Name = structType->Tag;
                tagSymbol->Type = specifier->Ic;
                _scope->Tags[structType->Tag] = tagSymbol;
            }

            delete declarationList;
        }

        return specifier;
    }
    else if (name == "EnumType")
    {
        CTypeSpecifier *specifier;

        if (tag == "Name")
        {
            IcTagSymbol *tagSymbol = _scope->LookupTag(_stack[stackIndex + 1].Text);

            specifier = new CTypeSpecifier;
            specifier->Type = CIcTypeSpecifier;

            if (tagSymbol)
            {
                if (tagSymbol->Type.Kind != IcEnumTypeKind)
                    throw "tag type mismatch";
            }

            specifier->Ic.Kind = IcBaseTypeKind;
            specifier->Ic.Traits = 0;
            specifier->Ic.Base.Type = IcIntType;
            specifier->Ic.Base.EnumTag = new std::string(_stack[stackIndex + 1].Text);
        }
        else if (tag == "Fields" || tag == "Fields2" || tag == "NameFields" || tag == "NameFields2")
        {
            IcEnumType *enumType;
            CEnumValueList *valueList;
            int nextValue = 0;

            specifier = new CTypeSpecifier;
            specifier->Type = CIcTypeSpecifier;
            specifier->Ic.Kind = IcEnumTypeKind;
            specifier->Ic.Traits = 0;
            enumType = new IcEnumType;
            specifier->Ic.Enum = enumType;

            if (tag == "Fields" || tag == "Fields2")
            {
                valueList = (CEnumValueList *)_stack[stackIndex + 2].Object;
            }
            else
            {
                enumType->Tag = _stack[stackIndex + 1].Text;
                valueList = (CEnumValueList *)_stack[stackIndex + 3].Object;
            }

            for (auto it = valueList->begin(); it != valueList->end(); ++it)
            {
                IcEnumField field;

                field.Name = it->Name;

                if (it->ValuePresent)
                    field.Value = it->Value;
                else
                    field.Value = nextValue;

                nextValue = field.Value + 1;

                enumType->Fields.push_back(field);

                if (_scope->Symbols.find(field.Name) != _scope->Symbols.end())
                    throw "symbol already in use";

                IcOrdinarySymbol *ordinarySymbol = new IcOrdinarySymbol;

                ordinarySymbol->Kind = IcConstantSymbolKind;
                ordinarySymbol->Name = field.Name;
                ordinarySymbol->c.Value = field.Value;
                _scope->Symbols[field.Name] = ordinarySymbol;
            }

            if (enumType->Tag.length() != 0)
            {
                if (_scope->Tags.find(enumType->Tag) != _scope->Tags.end())
                    throw "tag already in use";

                IcTagSymbol *tagSymbol = new IcTagSymbol;

                tagSymbol->Name = enumType->Tag;
                tagSymbol->Type = specifier->Ic;
                _scope->Tags[enumType->Tag] = tagSymbol;
            }

            delete valueList;
        }

        return specifier;
    }
    else if (name == "EnumValueList")
    {
        CEnumValueList *list;

        if (tag == "One")
        {
            CEnumValue *enumValue = (CEnumValue *)_stack[stackIndex].Object;

            list = new CEnumValueList;
            list->push_back(*enumValue);
            delete enumValue;
        }
        else if (tag == "Many")
        {
            CEnumValue *enumValue = (CEnumValue *)_stack[stackIndex + 2].Object;

            list = (CEnumValueList *)_stack[stackIndex].Object;
            list->push_back(*enumValue);
            delete enumValue;
        }

        return list;
    }
    else if (name == "EnumValue")
    {
        CEnumValue *enumValue;

        if (tag == "NoValue")
        {
            enumValue = new CEnumValue;
            enumValue->Name = _stack[stackIndex].Text;
            enumValue->ValuePresent = false;
        }
        else if (tag == "Value")
        {
            IcTypedValue convertedValue;
            IcImmediateValue value;

            ConvertOneType(_stack[stackIndex + 2].Value, 0, convertedValue);

            if (!CheckType(convertedValue.Type, CAllowInt) || !_evaluator.GetRegister(ReadValue(convertedValue, &_intType), value))
                throw "enum value must be a constant integer expression";

            enumValue = new CEnumValue;
            enumValue->Name = _stack[stackIndex].Text;
            enumValue->ValuePresent = true;
            enumValue->Value = value.u.Int;
        }

        return enumValue;
    }
    else if (name == "DeclarationSpecifierList")
    {
        CDeclaration *declaration;

        if (tag == "Type")
        {
            CTypeSpecifier *specifier = (CTypeSpecifier *)_stack[stackIndex].Object;

            declaration = new CDeclaration;
            declaration->Type = *specifier;
            declaration->TypeSpecified = true;
            delete specifier;
        }
        else if (tag == "ManyType")
        {
            CTypeSpecifier *specifier = (CTypeSpecifier *)_stack[stackIndex + 1].Object;

            declaration = (CDeclaration *)_stack[stackIndex].Object;

            if (!declaration->TypeSpecified)
            {
                declaration->Type = *specifier;
                declaration->TypeSpecified = true;
            }
            else
            {
                if (declaration->Type.Type != CBasicTypeSpecifier || specifier->Type != CBasicTypeSpecifier)
                    declaration /*DUMMY*/= declaration; // TODO: Error

                // TODO: Check for duplicate specifiers.

                if (specifier->Basic < CMaximumBasicType)
                {
                    if (!((unsigned int)declaration->Type.Basic & CBasicTypeMask))
                        declaration->Type.Basic = (CBasicType)((unsigned int)declaration->Type.Basic | specifier->Basic);
                    else
                        ; // TODO: Error
                }
                else
                {
                    if (specifier->Basic == CSignedType)
                    {
                        if (declaration->Type.Basic & CSignedType)
                            ; // TODO: Duplicate
                        else if (declaration->Type.Basic & CUnsignedType)
                            ; // TODO: Conflict
                        else
                            declaration->Type.Basic = (CBasicType)((unsigned int)declaration->Type.Basic | CSignedType);
                    }
                    else if (specifier->Basic == CUnsignedType)
                    {
                        if (declaration->Type.Basic & CSignedType)
                            ; // TODO: Conflict
                        else if (declaration->Type.Basic & CUnsignedType)
                            ; // TODO: Duplicate
                        else
                            declaration->Type.Basic = (CBasicType)((unsigned int)declaration->Type.Basic | CUnsignedType);
                    }
                }
            }

            delete specifier;
        }
        else if (tag == "Storage")
        {
            CStorageClass storage = (CStorageClass)(unsigned int)_stack[stackIndex].Object;

            declaration = new CDeclaration;
            declaration->Storage = storage;
        }
        else if (tag == "ManyStorage")
        {
            CStorageClass storage = (CStorageClass)(unsigned int)_stack[stackIndex + 1].Object;

            declaration = (CDeclaration *)_stack[stackIndex].Object;

            if ((unsigned int)declaration->Storage != 0)
            {
                throw "more than one storage class specified";
            }
            else
            {
                declaration->Storage = storage;
            }
        }
        else if (tag == "Qualifier")
        {
            CTypeQualifier qualifier = (CTypeQualifier)(unsigned int)_stack[stackIndex].Object;

            declaration = new CDeclaration;
            declaration->Qualifiers = qualifier;
        }
        else if (tag == "ManyQualifier")
        {
            CTypeQualifier qualifier = (CTypeQualifier)(unsigned int)_stack[stackIndex + 1].Object;

            declaration = (CDeclaration *)_stack[stackIndex].Object;

            if ((unsigned int)declaration->Qualifiers & (unsigned int)qualifier)
            {
                // TODO: Duplicate
            }
            else
            {
                declaration->Qualifiers = (CTypeQualifier)((unsigned int)declaration->Qualifiers | (unsigned int)qualifier);
            }
        }

        return declaration;
    }
    else if (name == "ParameterList")
    {
        CParameterList *parameterList;

        if (tag == "One")
        {
            parameterList = new CParameterList;
            parameterList->push_back(_stack[stackIndex].Value);
        }
        else if (tag == "Many")
        {
            parameterList = (CParameterList *)_stack[stackIndex].Object;
            parameterList->push_back(_stack[stackIndex + 2].Value);
        }

        return parameterList;
    }
    else if (name == "DeclaratorParameterList")
    {
        CDeclaratorParameterList *parameterList;

        if (tag == "NoEllipsis")
        {
            parameterList = (CDeclaratorParameterList *)_stack[stackIndex].Object;
        }
        else if (tag == "Ellipsis")
        {
            parameterList = (CDeclaratorParameterList *)_stack[stackIndex].Object;
            parameterList->Ellipsis = true;
        }

        return parameterList;
    }
    else if (name == "DeclaratorParameterList2")
    {
        CDeclaratorParameterList *parameterList;

        if (tag == "One")
        {
            CDeclaration *declaration = (CDeclaration *)_stack[stackIndex].Object;

            parameterList = new CDeclaratorParameterList;
            parameterList->List.push_back(*declaration);
            delete declaration;
        }
        else if (tag == "Many")
        {
            CDeclaration *declaration = (CDeclaration *)_stack[stackIndex + 2].Object;

            parameterList = (CDeclaratorParameterList *)_stack[stackIndex].Object;
            parameterList->List.push_back(*declaration);
            delete declaration;
        }

        return parameterList;
    }
    else if (name == "DeclaratorParameter")
    {
        CDeclaration *declaration;

        if (tag == "None")
        {
            declaration = (CDeclaration *)_stack[stackIndex].Object;
        }
        else if (tag == "Name")
        {
            CDeclarator *declarator = (CDeclarator *)_stack[stackIndex + 1].Object;

            declaration = (CDeclaration *)_stack[stackIndex].Object;
            declaration->Declarator = *declarator;
            delete declarator;
        }
        else if (tag == "NoName")
        {
            CDeclarator *declarator = (CDeclarator *)_stack[stackIndex + 1].Object;

            declaration = (CDeclaration *)_stack[stackIndex].Object;
            declaration->Declarator = *declarator;
            delete declarator;
        }

        return declaration;
    }
    else if (name == "Declaration" || name == "StructDeclaration")
    {
        CDeclarationList *list;

        if (tag == "NoName")
        {
            CDeclaration *declaration = (CDeclaration *)_stack[stackIndex].Object;

            list = new CDeclarationList;
            list->push_back(*declaration);
            delete declaration;
        }
        else if (tag == "Name")
        {
            CDeclaration *declaration = (CDeclaration *)_stack[stackIndex].Object;
            CInitializationDeclaratorList *initDeclList = (CInitializationDeclaratorList *)_stack[stackIndex + 1].Object;

            list = new CDeclarationList;

            for (auto it = initDeclList->begin(); it != initDeclList->end(); ++it)
            {
                CDeclaration newDeclaration = *declaration;

                newDeclaration.Declarator = *it->Declarator;
                newDeclaration.Initializer = it->Initializer;
                newDeclaration.Bits = it->Bits;
                list->push_back(newDeclaration);

                it->Initializer = NULL;
            }

            delete initDeclList;
        }

        return list;
    }
    else if (name == "DeclarationList" || name == "StructDeclarationList")
    {
        CDeclarationList *list;

        if (tag == "One")
        {
            CDeclarationList *subList = (CDeclarationList *)_stack[stackIndex].Object;

            list = subList;
        }
        else if (tag == "Many")
        {
            CDeclarationList *subList = (CDeclarationList *)_stack[stackIndex + 1].Object;

            list = (CDeclarationList *)_stack[stackIndex].Object;
            list->insert(list->end(), subList->begin(), subList->end());
            delete subList;
        }

        return list;
    }
    else if (name == "InitializationDeclarator" || name == "StructDeclarator")
    {
        CInitializationDeclarator *initDecl;

        if (tag == "Name") // InitializationDeclarator and StructDeclarator
        {
            CDeclarator *declarator = (CDeclarator *)_stack[stackIndex].Object;

            initDecl = new CInitializationDeclarator;
            initDecl->Declarator = declarator;

            SequencePoint();
        }
        else if (tag == "NameValue") // InitializationDeclarator
        {
            CDeclarator *declarator = (CDeclarator *)_stack[stackIndex].Object;
            CInitializer *initializer = (CInitializer *)_stack[stackIndex + 2].Object;

            initDecl = new CInitializationDeclarator;
            initDecl->Declarator = declarator;
            initDecl->Initializer = initializer;

            SequencePoint();
        }
        else if (tag == "NameBits") // StructDeclarator
        {
            CDeclarator *declarator = (CDeclarator *)_stack[stackIndex].Object;

            initDecl = new CInitializationDeclarator;
            initDecl->Declarator = declarator;
            initDecl->Bits = 0; // TODO
        }
        else if (tag == "Bits") // StructDeclarator
        {
            CDeclarator *declarator = (CDeclarator *)_stack[stackIndex].Object;

            initDecl = new CInitializationDeclarator;
            initDecl->Bits = 0; // TODO
        }

        return initDecl;
    }
    else if (name == "InitializationDeclaratorList" || name == "StructDeclaratorList")
    {
        CInitializationDeclaratorList *list;

        if (tag == "One")
        {
            CInitializationDeclarator *initDecl = (CInitializationDeclarator *)_stack[stackIndex].Object;

            list = new CInitializationDeclaratorList;
            list->push_back(*initDecl);
            delete initDecl;
        }
        else if (tag == "Many")
        {
            CInitializationDeclarator *initDecl = (CInitializationDeclarator *)_stack[stackIndex + 2].Object;

            list = (CInitializationDeclaratorList *)_stack[stackIndex].Object;
            list->push_back(*initDecl);
            delete initDecl;
        }

        return list;
    }
    else if (name == "Initializer")
    {
        CInitializer *initializer;

        if (tag == "Basic")
        {
            initializer = new CInitializer;
            initializer->IsList = false;
            initializer->Value = new IcTypedValue(_stack[stackIndex].Value);
        }
        else if (tag == "Aggregate" || tag == "Aggregate2")
        {
            initializer = new CInitializer;
            initializer->IsList = true;
            initializer->List = (CInitializerList *)_stack[stackIndex + 1].Object;
        }

        return initializer;
    }
    else if (name == "InitializerList")
    {
        CInitializerList *list;

        if (tag == "One")
        {
            CInitializer *initializer = (CInitializer *)_stack[stackIndex].Object;

            list = new CInitializerList;
            list->push_back(*initializer);
            delete initializer;
        }
        else if (tag == "Many")
        {
            CInitializer *initializer = (CInitializer *)_stack[stackIndex + 2].Object;

            list = (CInitializerList *)_stack[stackIndex].Object;
            list->push_back(*initializer);
            delete initializer;
        }

        return list;
    }
    else if (name == "FunctionDefinition")
    {
        CDeclaration *declaration = (CDeclaration *)_stack[stackIndex].Object;
        CDeclarator *declarator = (CDeclarator *)_stack[stackIndex + 1].Object;

        delete declaration;
        delete declarator;
    }
    else if (name == "Statement")
    {
        if (tag == "Definition")
        {
            CDeclarationList *declarationList = (CDeclarationList *)_stack[stackIndex].Object;

            DefinitionList(declarationList);

            delete declarationList;
        }

        SequencePoint();
    }
    else if (name == "IfStatement")
    {
        CIfStatementContext *context = (CIfStatementContext *)_stack[stackIndex].Object;

        delete context;
    }
    else if (name == "WhileStatement")
    {
        CWhileStatementContext *context = (CWhileStatementContext *)_stack[stackIndex].Object;

        delete context;
    }
    else if (name == "DoWhileStatement")
    {
        CDoWhileStatementContext *context = (CDoWhileStatementContext *)_stack[stackIndex].Object;

        delete context;
    }
    else if (name == "ForStatement")
    {
        CForStatementContext *context = (CForStatementContext *)_stack[stackIndex].Object;

        delete context;
    }
    else if (name == "SwitchStatement")
    {
        CSwitchStatementContext *context = (CSwitchStatementContext *)_stack[stackIndex].Object;

        delete context;
    }
    else if (name == "ContinueStatement")
    {
        IcControlScope *controlScope = _controlScope;

        while (controlScope && !controlScope->ContinueTarget)
            controlScope = controlScope->Parent;

        if (!controlScope)
            throw "illegal 'continue' statement";

        IcOperation op;

        op.Code = IcOpJmp;
        op.b.B1 = controlScope->ContinueTarget;
        AppendOp(op);

        _currentBlock = _functionScope->CreateBlock();
    }
    else if (name == "BreakStatement")
    {
        IcControlScope *controlScope = _controlScope;

        while (controlScope && !controlScope->BreakTarget)
            controlScope = controlScope->Parent;

        if (!controlScope)
            throw "illegal 'break' statement";

        IcOperation op;

        op.Code = IcOpJmp;
        op.b.B1 = controlScope->BreakTarget;
        AppendOp(op);

        _currentBlock = _functionScope->CreateBlock();
    }
    else if (name == "ReturnStatement")
    {
        IcOperation op;

        if (tag == "NoValue")
        {
            if (!CheckType(_functionScope->Type.Function->ReturnType, CAllowVoid))
                throw "return statement without expression used in function with non-void return type";
        }
        else if (tag == "Value")
        {
            IcTypedValue returnVariable;
            IcTypedValue dummy;

            if (CheckType(_functionScope->Type.Function->ReturnType, CAllowVoid))
                throw "return statement with expression used in function with void return type";

            returnVariable = GetVariable(_functionScope->ReturnVariableId);
            AssignExpressionAssign(returnVariable, _stack[stackIndex + 1].Value, dummy, true);
        }
        else
        {
            assert(false);
        }

        op.Code = IcOpJmp;
        op.b.B1 = _functionScope->EndBlock;
        AppendOp(op);

        _currentBlock = _functionScope->CreateBlock();
    }
    else if (name == "GotoStatement")
    {
        std::string &name = _stack[stackIndex + 1].Text;
        IcOperation op;
        auto blockIt = _functionScope->Labels.find(name);

        op.Code = IcOpJmp;
        op.b.B1 = blockIt != _functionScope->Labels.end() ? blockIt->second : NULL;
        AppendOp(op);

        if (!op.b.B1)
        {
            _functionScope->PendingGotos.insert(IcLabelBlockMap::value_type(name, _currentBlock));
        }

        _currentBlock = _functionScope->CreateBlock();
    }
    else if (name == "ExpressionStatement")
    {
        if (tag == "NonEmpty")
        {
            _value = _stack[stackIndex].Value;
        }
    }
    else if (name == "Expression")
    {
        _value = _stack[stackIndex].Value;
    }
    else if (name == "CommaExpression")
    {
        if (tag == "Up")
        {
            _value = _stack[stackIndex].Value;
        }
        else if (tag == "Comma")
        {
            _value = _stack[stackIndex + 3].Value;
        }
    }
    else if (name == "AssignExpression")
    {
        IcTypedValue newValue;

        if (tag == "Up")
        {
            _value = _stack[stackIndex].Value;
        }
        else if (tag == "Assign")
        {
            AssignExpressionAssign(_stack[stackIndex].Value, _stack[stackIndex + 2].Value, _value);
        }
        else if (tag == "Add")
        {
            AddExpressionAdd(_stack[stackIndex].Value, _stack[stackIndex + 2].Value, newValue);
            AssignExpressionAssign(_stack[stackIndex].Value, newValue, _value);
        }
        else if (tag == "Subtract")
        {
            AddExpressionSubtract(_stack[stackIndex].Value, _stack[stackIndex + 2].Value, newValue);
            AssignExpressionAssign(_stack[stackIndex].Value, newValue, _value);
        }
        else if (tag == "Multiply" || tag == "Divide" || tag == "Modulo")
        {
            MultiplyExpression(tag, _stack[stackIndex].Value, _stack[stackIndex + 2].Value, newValue);
            AssignExpressionAssign(_stack[stackIndex].Value, newValue, _value);
        }
        else if (tag == "And")
        {
            AndExpression(_stack[stackIndex].Value, _stack[stackIndex + 2].Value, newValue);
            AssignExpressionAssign(_stack[stackIndex].Value, newValue, _value);
        }
        else if (tag == "Or")
        {
            OrExpression(_stack[stackIndex].Value, _stack[stackIndex + 2].Value, newValue);
            AssignExpressionAssign(_stack[stackIndex].Value, newValue, _value);
        }
        else if (tag == "Xor")
        {
            XorExpression(_stack[stackIndex].Value, _stack[stackIndex + 2].Value, newValue);
            AssignExpressionAssign(_stack[stackIndex].Value, newValue, _value);
        }
        else if (tag == "LeftShift" || tag == "RightShift")
        {
            ShiftExpression(tag, _stack[stackIndex].Value, _stack[stackIndex + 2].Value, newValue);
            AssignExpressionAssign(_stack[stackIndex].Value, newValue, _value);
        }
    }
    else if (name == "ConditionalExpression")
    {
        if (tag == "Up")
        {
            _value = _stack[stackIndex].Value;
        }
        else if (tag == "Conditional")
        {
            CConditionalExpressionContext *context = (CConditionalExpressionContext *)_stack[stackIndex + 1].Object;
            IcTypedValue value1;
            IcTypedValue value2;
            IcType commonType;
            IcOperation op;

            ConvertOneType(_stack[stackIndex + 3].Value, 0, value1);
            ConvertOneType(_stack[stackIndex + 6].Value, 0, value2);

            if (CheckType(value1.Type, CAllowInt | CAllowFloat) && CheckType(value2.Type, CAllowInt | CAllowFloat))
            {
                CommonNumberType(value1.Type, value2.Type, commonType);
            }
            else if (CheckType(value1.Type, CAllowPointer) && CheckType(value2.Type, CAllowPointer))
            {
                if (!CheckCompatiblePointerTypes(value1.Type, value2.Type, true))
                    throw "pointer type mismatch in ternary conditional operator";

                commonType = value1.Type;
            }
            else if ((CheckType(value1.Type, CAllowPointer) && CheckType(value2.Type, CAllowInt) && IsConstantZero(value2.Value)) ||
                (CheckType(value1.Type, CAllowInt) && IsConstantZero(value1.Value) && CheckType(value2.Type, CAllowPointer)))
            {
                commonType = CheckType(value1.Type, CAllowPointer) ? value1.Type : value2.Type;
            }
            else if (CheckType(value1.Type, CAllowStruct) && CheckType(value2.Type, CAllowStruct))
            {
                if (!value1.Type.EqualTo(value2.Type))
                    throw "type mismatch in ternary conditional operator";

                commonType = value1.Type;
            }
            else if (CheckType(value1.Type, CAllowVoid) && CheckType(value2.Type, CAllowVoid))
            {
                commonType = value1.Type;
            }
            else
            {
                throw "type mismatch in ternary conditional operator";
            }

            IcRegister addressRegister = 0;
            IcRegister convertedValue1 = 0;
            IcRegister convertedValue2 = 0;

            if (!SideEffectsDisabled())
            {
                int size = commonType.SizeOf();

                if (size == -1)
                    throw "cannot calculate size of type";

                addressRegister = AllocateRegister();
                op.Code = IcOpLdF;
                op.Mode = IcIntPtrType;
                op.cr.C1 = CreateImmediateNumber(_functionScope->AllocateSlot(size), IcIntType);
                op.cr.R2 = addressRegister;
                context->StartBlock->Operations.insert(context->StartBlock->Operations.end() - 1, op);

                if ((commonType.Kind == IcBaseTypeKind && commonType.Base.Type != IcVoidType) || commonType.Kind == IcPointerTypeKind)
                {
                    op = IcOperation();
                    op.Code = IcOpSt;
                    op.Mode = GetBaseType(commonType);
                    op.rr.R2 = addressRegister;

                    _currentBlock = context->NonZeroBlock;
                    convertedValue1 = ReadValue(value1, &commonType);
                    op.rr.R1 = convertedValue1;
                    AppendOp(op);

                    _currentBlock = context->ZeroBlock;
                    convertedValue2 = ReadValue(value2, &commonType);
                    op.rr.R1 = convertedValue2;
                    AppendOp(op);
                }
                else if (commonType.Kind == IcStructTypeKind)
                {
                    assert(value1.Value.Kind == IcIndirectStorageKind);
                    assert(value2.Value.Kind == IcIndirectStorageKind);
                    op = IcOperation();
                    op.Code = IcOpCpyI;
                    op.rrc.R2 = addressRegister;
                    op.rrc.C3 = CreateImmediateNumber(size, IcIntType);

                    _currentBlock = context->NonZeroBlock;
                    op.rrc.R1 = value1.Value.i.Address;
                    AppendOp(op);

                    _currentBlock = context->ZeroBlock;
                    op.rrc.R1 = value2.Value.i.Address;
                    AppendOp(op);
                }

                op = IcOperation();
                op.Code = IcOpJmp;
                op.b.B1 = context->NextBlock;

                _currentBlock = context->NonZeroBlock;
                AppendOp(op);

                _currentBlock = context->ZeroBlock;
                AppendOp(op);

                _currentBlock = context->NextBlock;
            }

            if ((commonType.Kind == IcBaseTypeKind && commonType.Base.Type != IcVoidType) || commonType.Kind == IcPointerTypeKind)
            {
                _value.Type = commonType;
                _value.Value.Kind = IcRegisterStorageKind;
                _value.Value.r.Register = AllocateRegister();

                if (!SideEffectsDisabled())
                {
                    op = IcOperation();
                    op.Code = IcOpLd;
                    op.Mode = GetBaseType(commonType);
                    op.rr.R1 = addressRegister;
                    op.rr.R2 = _value.Value.r.Register;
                    AppendOp(op);
                }

                // Calculate a constant value if we can.

                IcImmediateValue constant0;
                IcImmediateValue constant1;
                IcImmediateValue constant2;

                if (convertedValue1 == 0)
                    convertedValue1 = ReadValue(value1, &commonType);
                if (convertedValue2 == 0)
                    convertedValue2 = ReadValue(value2, &commonType);

                if (_stack[stackIndex].Value.Value.Kind == IcRegisterStorageKind &&
                    _evaluator.GetRegister(_stack[stackIndex].Value.Value.r.Register, constant0) &&
                    _evaluator.GetRegister(convertedValue1, constant1) &&
                    _evaluator.GetRegister(convertedValue2, constant2))
                {
                    _evaluator.SetRegister(_value.Value.r.Register, IsConstantZero(constant0) ? constant2 : constant1);
                }
            }
            else if (commonType.Kind == IcStructTypeKind)
            {
                _value.Type = commonType;
                _value.Value.Kind = IcIndirectStorageKind;
                _value.Value.i.Address = addressRegister;
            }
            else
            {
                assert(commonType.Kind == IcBaseTypeKind && commonType.Base.Type == IcVoidType);
                _value.Type = commonType;
                _value.Value.Kind = IcNoStorageKind;
            }

            delete context;
        }
    }
    else if (name == "AndAndExpression" || name == "OrOrExpression")
    {
        if (tag == "Up")
        {
            _value = _stack[stackIndex].Value;
        }
        else if (tag == "AndAnd" || tag == "OrOr")
        {
            _value.Type = _intType;
            _value.Value.Kind = IcRegisterStorageKind;
            _value.Value.r.Register = AllocateRegister();

            if (!SideEffectsDisabled())
            {
                CLogicalExpressionContext *context = (CLogicalExpressionContext *)_stack[stackIndex + 1].Object;
                IcOperation op;

                op.Code = IcOpLd;
                op.Mode = IcIntType;
                op.rr.R1 = context->AddressRegister;
                op.rr.R2 = _value.Value.r.Register;
                AppendOp(op);

                delete context;
            }

            // Calculate a constant value if we can.

            IcImmediateValue constant1;
            IcImmediateValue constant2;

            if (_stack[stackIndex].Value.Value.Kind == IcRegisterStorageKind &&
                _evaluator.GetRegister(_stack[stackIndex].Value.Value.r.Register, constant1) &&
                _stack[stackIndex + 3].Value.Value.Kind == IcRegisterStorageKind &&
                _evaluator.GetRegister(_stack[stackIndex + 3].Value.Value.r.Register, constant2))
            {
                if (tag == "OrOr")
                    _evaluator.SetRegister(_value.Value.r.Register, CreateImmediateNumber(!(IsConstantZero(constant1) && IsConstantZero(constant2)), IcIntType));
                else
                    _evaluator.SetRegister(_value.Value.r.Register, CreateImmediateNumber(!(IsConstantZero(constant1) || IsConstantZero(constant2)), IcIntType));
            }
        }
    }
    else if (name == "OrExpression")
    {
        if (tag == "Up")
        {
            _value = _stack[stackIndex].Value;
        }
        else if (tag == "Or")
        {
            OrExpression(_stack[stackIndex].Value, _stack[stackIndex + 2].Value, _value);
        }
    }
    else if (name == "XorExpression")
    {
        if (tag == "Up")
        {
            _value = _stack[stackIndex].Value;
        }
        else if (tag == "Xor")
        {
            XorExpression(_stack[stackIndex].Value, _stack[stackIndex + 2].Value, _value);
        }
    }
    else if (name == "AndExpression")
    {
        if (tag == "Up")
        {
            _value = _stack[stackIndex].Value;
        }
        else if (tag == "And")
        {
            AndExpression(_stack[stackIndex].Value, _stack[stackIndex + 2].Value, _value);
        }
    }
    else if (name == "EqualityExpression")
    {
        if (tag == "Up")
        {
            _value = _stack[stackIndex].Value;
        }
        else if (tag == "Equal" || tag == "NotEqual")
        {
            EqualityExpression(tag, _stack[stackIndex].Value, _stack[stackIndex + 2].Value, _value);
        }
    }
    else if (name == "RelationalExpression")
    {
        if (tag == "Up")
        {
            _value = _stack[stackIndex].Value;
        }
        else if (tag == "Less" || tag == "LessEqual" || tag == "Greater" || tag == "GreaterEqual")
        {
            RelationalExpression(tag, _stack[stackIndex].Value, _stack[stackIndex + 2].Value, _value);
        }
    }
    else if (name == "ShiftExpression")
    {
        if (tag == "Up")
        {
            _value = _stack[stackIndex].Value;
        }
        else if (tag == "LeftShift" || tag == "RightShift")
        {
            ShiftExpression(tag, _stack[stackIndex].Value, _stack[stackIndex + 2].Value, _value);
        }
    }
    else if (name == "AddExpression")
    {
        if (tag == "Up")
        {
            _value = _stack[stackIndex].Value;
        }
        else if (tag == "Add")
        {
            AddExpressionAdd(_stack[stackIndex].Value, _stack[stackIndex + 2].Value, _value);
        }
        else if (tag == "Subtract")
        {
            AddExpressionSubtract(_stack[stackIndex].Value, _stack[stackIndex + 2].Value, _value);
        }
    }
    else if (name == "MultiplyExpression")
    {
        if (tag == "Up")
        {
            _value = _stack[stackIndex].Value;
        }
        else if (tag == "Multiply" || tag == "Divide" || tag == "Modulo")
        {
            MultiplyExpression(tag, _stack[stackIndex].Value, _stack[stackIndex + 2].Value, _value);
        }
    }
    else if (name == "CastExpression")
    {
        if (tag == "Up")
        {
            _value = _stack[stackIndex].Value;
        }
        else if (tag == "Cast")
        {
            CDeclaration *declaration = (CDeclaration *)_stack[stackIndex + 1].Object;
            CDeclarator declarator;
            IcType type = CreateIcType(declaration->Type, declaration->Qualifiers, declarator, declaration->TypeSpecified);

            CastExpression(type, _stack[stackIndex + 4].Value, _value);

            delete declaration;
        }
        else if (tag == "Cast2")
        {
            CDeclaration *declaration = (CDeclaration *)_stack[stackIndex + 1].Object;
            CDeclarator *declarator = (CDeclarator *)_stack[stackIndex + 2].Object;
            IcType type = CreateIcType(declaration->Type, declaration->Qualifiers, *declarator, declaration->TypeSpecified);

            CastExpression(type, _stack[stackIndex + 5].Value, _value);

            delete declaration;
            delete declarator;
        }
    }
    else if (name == "UnaryExpression")
    {
        if (tag == "Up")
        {
            _value = _stack[stackIndex].Value;
        }
        else if (tag == "Increment")
        {
            IcTypedValue oneValue;
            IcTypedValue result;

            oneValue = LoadInt(1, IcIntType);
            AddExpressionAdd(_stack[stackIndex + 1].Value, oneValue, result);
            AssignExpressionAssign(_stack[stackIndex + 1].Value, result, _value);
        }
        else if (tag == "Decrement")
        {
            IcTypedValue oneValue;
            IcTypedValue result;

            oneValue = LoadInt(1, IcIntType);
            AddExpressionSubtract(_stack[stackIndex + 1].Value, oneValue, result);
            AssignExpressionAssign(_stack[stackIndex + 1].Value, result, _value);
        }
        else if (tag == "Positive")
        {
            IcTypedValue value1;

            ConvertOneType(_stack[stackIndex + 1].Value, 0, value1);

            if (!CheckType(value1.Type, CAllowInt | CAllowFloat | CAllowPointer))
                throw "invalid type for '+' unary operator";

            _value = value1;
        }
        else if (tag == "Negative")
        {
            IcOperation op;
            IcTypedValue value1;

            ConvertOneType(_stack[stackIndex + 1].Value, 0, value1);

            if (!CheckType(value1.Type, CAllowInt | CAllowFloat))
                throw "invalid type for '-' unary operator";

            _value.Type = value1.Type;
            _value.Value.Kind = IcRegisterStorageKind;
            _value.Value.r.Register = AllocateRegister();

            op.Code = IcOpSubIR;
            op.Mode = GetBaseType(_value.Type);
            op.rcr.R1 = ReadValue(value1);
            op.rcr.C2 = CreateImmediateNumber(0, _value.Type.Base.Type);
            op.rcr.R3 = _value.Value.r.Register;
            AppendOp(op);
        }
        else if (tag == "LogicalNot")
        {
            IcOperation op;
            IcTypedValue value1;

            ConvertOneType(_stack[stackIndex + 1].Value, 0, value1);

            if (!CheckType(value1.Type, CAllowInt | CAllowFloat | CAllowPointer))
                throw "invalid type for '!' unary operator";

            _value.Type.Kind = IcBaseTypeKind;
            _value.Type.Base.Type = IcIntType;
            _value.Value.Kind = IcRegisterStorageKind;
            _value.Value.r.Register = AllocateRegister();

            op.Code = IcOpChEI;
            op.Mode = GetBaseType(value1.Type);
            op.rcccr.R1 = ReadValue(value1);
            op.rcccr.C2 = CreateImmediateNumber(0, _value.Type.Base.Type);
            op.rcccr.C3 = CreateImmediateNumber(1, _value.Type.Base.Type);
            op.rcccr.C4 = CreateImmediateNumber(0, _value.Type.Base.Type);
            op.rcccr.R5 = _value.Value.r.Register;
            AppendOp(op);
        }
        else if (tag == "Not")
        {
            IcOperation op;
            IcTypedValue value1;

            ConvertOneType(_stack[stackIndex + 1].Value, 0, value1);

            if (!CheckType(value1.Type, CAllowInt))
                throw "invalid type for '~' unary operator";

            _value.Type = value1.Type;
            _value.Value.Kind = IcRegisterStorageKind;
            _value.Value.r.Register = AllocateRegister();

            op.Code = IcOpNot;
            op.Mode = GetBaseType(_value.Type);
            op.rr.R1 = ReadValue(value1);
            op.rr.R2 = _value.Value.r.Register;
            AppendOp(op);
        }
        else if (tag == "Dereference")
        {
            IcTypedValue value1;

            ConvertOneType(_stack[stackIndex + 1].Value, 0, value1);

            if (!CheckType(value1.Type, CAllowPointer))
                throw "invalid type for '*' unary operator";
            if (value1.Type.Pointer.Child->Kind == IcBaseTypeKind && value1.Type.Pointer.Child->Base.Type == IcVoidType)
                throw "cannot dereference pointer to void";

            if (value1.Type.Pointer.Child->Kind == IcFunctionTypeKind)
            {
                _value = value1;

                if (_value.Value.Kind = IcRegisterStorageKind)
                    _value.Value.r.FunctionDesignator = true;
            }
            else
            {
                _value.Type = GetChildForPointerType(value1.Type);
                _value.Value.Kind = IcIndirectStorageKind;
                _value.Value.i.Address = ReadValue(value1);
                _value.Value.i.LValue = true;
            }
        }
        else if (tag == "AddressOf")
        {
            IcTypedValue value1;

            ConvertOneType(_stack[stackIndex + 1].Value, CDontConvertArray, value1);

            if (!CheckType(value1.Type, CAllowInt | CAllowFloat | CAllowPointer | CAllowArray | CAllowStruct))
                throw "invalid type for '&' unary operator";

            if (value1.Value.Kind == IcRegisterStorageKind)
            {
                if (value1.Value.r.FunctionDesignator)
                {
                    _value = value1;
                    _value.Value.r.FunctionDesignator = false;
                }
                else
                {
                    throw "address-of operator used on non-l-value";
                }
            }
            else
            {
                if (value1.Value.i.LValue)
                {
                    _value.Type.Kind = IcPointerTypeKind;
                    _value.Type.Pointer.Child = new IcType(value1.Type);
                    _value.Value.Kind = IcRegisterStorageKind;
                    _value.Value.r.Register = value1.Value.i.Address;
                }
                else
                {
                    throw "address-of operator used on non-l-value";
                }
            }
        }
        else if (tag == "SizeOfValue")
        {
            IcTypedValue &value = _stack[stackIndex + 2].Value;

            if (value.Value.Kind == IcRegisterStorageKind && value.Value.r.FunctionDesignator)
                throw "cannot calculate size of function";

            int size = value.Type.SizeOf();

            if (size == -1)
                throw "cannot calculate size of type";

            _value = LoadInt(size, _sizeT.Base.Type);
            _value.Type = _sizeT;
        }
        else if (tag == "SizeOfType" || tag == "SizeOfType2")
        {
            CDeclaration *declaration = (CDeclaration *)_stack[stackIndex + 3].Object;

            if (tag == "SizeOfType2")
            {
                CDeclarator *declarator = (CDeclarator *)_stack[stackIndex + 4].Object;

                declaration->Declarator = *declarator;
                delete declarator;
            }

            IcType type = CreateIcType(declaration->Type, declaration->Qualifiers, declaration->Declarator, declaration->TypeSpecified);
            int size = type.SizeOf();

            if (size == -1)
                throw "cannot calculate size of type";

            _value = LoadInt(size, _sizeT.Base.Type);
            _value.Type = _sizeT;

            delete declaration;
        }
    }
    else if (name == "PrimaryExpression")
    {
        if (tag == "Id")
        {
            IcOrdinarySymbol *ordinarySymbol = _scope->LookupSymbol(_stack[stackIndex].Text);

            if (!ordinarySymbol)
                throw "undefined symbol";

            if (ordinarySymbol->Kind == IcTypedefSymbolKind)
            {
                throw "invalid use of type as expression";
            }
            else if (ordinarySymbol->Kind == IcVariableSymbolKind)
            {
                _value = GetVariable(ordinarySymbol->v.Id);
            }
            else if (ordinarySymbol->Kind == IcConstantSymbolKind)
            {
                _value = LoadInt(ordinarySymbol->c.Value, IcIntType);
            }
            else if (ordinarySymbol->Kind == IcObjectSymbolKind)
            {
                IcOperation op;

                op.Code = IcOpLdI;
                op.Mode = IcIntPtrType;
                op.cr.C1.Type = IcReferenceType;
                op.cr.C1.u.Reference = ordinarySymbol->o.Reference;
                op.cr.C1.u.ReferenceOffset = 0;
                op.cr.R2 = AllocateRegister();
                AppendOp(op);

                if (ordinarySymbol->o.Type.Kind == IcFunctionTypeKind)
                {
                    _value.Type.Kind = IcPointerTypeKind;
                    _value.Type.Pointer.Child = new IcType(ordinarySymbol->o.Type);
                    _value.Value.Kind = IcRegisterStorageKind;
                    _value.Value.r.Register = op.cr.R2;
                    _value.Value.r.FunctionDesignator = true;
                }
                else
                {
                    _value.Type = ordinarySymbol->o.Type;
                    _value.Value.Kind = IcIndirectStorageKind;
                    _value.Value.i.Address = op.cr.R2;
                    _value.Value.i.LValue = true;
                }
            }
        }
        else if (tag == "Number")
        {
            IcOperation op;
            IcImmediateValue value;
            IcType type;

            value = CreateImmediateNumber(_stack[stackIndex].Text, &type);

            _value.Type = type;
            _value.Value.Kind = IcRegisterStorageKind;
            _value.Value.r.Register = AllocateRegister();

            op.Code = IcOpLdI;
            op.Mode = value.Type;
            op.cr.C1 = value;
            op.cr.R2 = _value.Value.r.Register;
            AppendOp(op);
        }
        else if (tag == "Char")
        {
            IcOperation op;
            IcImmediateValue value;

            value = CreateImmediateChar(_stack[stackIndex].Text);

            _value.Type.Kind = IcBaseTypeKind;
            _value.Type.Base.Type = value.Type;
            _value.Value.Kind = IcRegisterStorageKind;
            _value.Value.r.Register = AllocateRegister();

            op.Code = IcOpLdI;
            op.Mode = value.Type;
            op.cr.C1 = value;
            op.cr.R2 = _value.Value.r.Register;
            AppendOp(op);
        }
        else if (tag == "String")
        {
            IcOperation op;
            IcImmediateValue value;

            value = CreateImmediateString(_stack[stackIndex].Text, &_value.Type);

            _value.Value.Kind = IcIndirectStorageKind;
            _value.Value.i.Address = AllocateRegister();

            op.Code = IcOpLdI;
            op.Mode = IcIntPtrType;
            op.cr.C1 = value;
            op.cr.R2 = _value.Value.i.Address;
            AppendOp(op);
        }
        else if (tag == "Group")
        {
            _value = _stack[stackIndex + 1].Value;
        }
        else if (tag == "Increment" || tag == "Decrement")
        {
            IcTypedValue value;

            ConvertOneType(_stack[stackIndex].Value, 0, value);

            if (!CheckType(value.Type, CAllowInt | CAllowFloat | CAllowPointer))
                throw "invalid type for '++' or '--' postfix operator";
            if (value.Value.Kind != IcIndirectStorageKind || !value.Value.i.LValue)
                throw "'++' or '--' postfix operator used on non-l-value";

            _value = value;

            if (!_currentBlock)
            {
                // Force non-constant value.
                _value.Value.Kind = IcRegisterStorageKind;
                _value.Value.r.Register = AllocateRegister();
                return NULL;
            }

            if (SideEffectsDisabled())
                return NULL;

            CPendingSideEffect sideEffect;

            sideEffect.Type = tag == "Increment" ? CPendingIncrementEffect : CPendingDecrementEffect;
            sideEffect.Value = value;
            _pendingSideEffects.push_back(sideEffect);
        }
        else if (tag == "Call")
        {
            CallExpression(_stack[stackIndex].Value, NULL, _value);
        }
        else if (tag == "Call2")
        {
            CParameterList *parameterList = (CParameterList *)_stack[stackIndex + 2].Object;

            CallExpression(_stack[stackIndex].Value, parameterList, _value);
            delete parameterList;
        }
        else if (tag == "Array")
        {
            IcTypedValue newValue;
            IcType pointerType;

            AddExpressionAdd(_stack[stackIndex].Value, _stack[stackIndex + 2].Value, newValue, true, &pointerType);

            assert(pointerType.Kind == IcPointerTypeKind);
            _value.Type = GetChildForPointerType(pointerType);
            _value.Value.Kind = IcIndirectStorageKind;
            _value.Value.i.Address = ReadValue(newValue);
            _value.Value.i.LValue = true;
        }
        else if (tag == "Member")
        {
            IcOperation op;
            IcTypedValue value1;

            ConvertOneType(_stack[stackIndex].Value, 0, value1);

            if (!CheckType(value1.Type, CAllowStruct))
                throw "invalid type for '.' operator";

            assert(value1.Value.Kind == IcIndirectStorageKind);

            int offset;
            IcStructField *field = value1.Type.Struct->FindField(_stack[stackIndex + 2].Text, &offset);

            if (!field)
                throw "no such struct field";

            _value.Type = field->Type;
            _value.Value.Kind = IcIndirectStorageKind;
            _value.Value.i.Address = value1.Value.i.Address;
            _value.Value.i.LValue = value1.Value.i.LValue;

            if (offset != 0)
            {
                _value.Value.i.Address = AllocateRegister();

                op.Code = IcOpAddI;
                op.Mode = IcIntPtrType;
                op.rcr.R1 = value1.Value.i.Address;
                op.rcr.C2 = CreateImmediateNumber(offset, IcIntPtrType);
                op.rcr.R3 = _value.Value.i.Address;
                AppendOp(op);
            }
        }
        else if (tag == "IndirectMember")
        {
            IcOperation op;
            IcTypedValue value1;

            ConvertOneType(_stack[stackIndex].Value, 0, value1);

            if (!CheckType(value1.Type, CAllowPointer))
                throw "invalid type for '->' operator";

            IcType structType = GetChildForPointerType(value1.Type);

            if (structType.Kind != IcStructTypeKind)
                throw "invalid type for '->' operator";

            int offset;
            IcStructField *field = structType.Struct->FindField(_stack[stackIndex + 2].Text, &offset);

            if (!field)
                throw "no such struct field";

            if (offset != 0)
            {
                _value.Type = field->Type;
                _value.Value.Kind = IcIndirectStorageKind;
                _value.Value.i.Address = AllocateRegister();
                _value.Value.i.LValue = true;

                op.Code = IcOpAddI;
                op.Mode = IcIntPtrType;
                op.rcr.R1 = ReadValue(value1);
                op.rcr.C2 = CreateImmediateNumber(offset, IcIntPtrType);
                op.rcr.R3 = _value.Value.i.Address;
                AppendOp(op);
            }
            else
            {
                _value.Type = field->Type;
                _value.Value.Kind = IcIndirectStorageKind;
                _value.Value.i.Address = ReadValue(value1);
                _value.Value.i.LValue = true;
            }
        }
    }
    else if (name == "ProgramElement")
    {
        if (tag == "Definition")
        {
            CDeclarationList *declarationList = (CDeclarationList *)_stack[stackIndex].Object;

            DefinitionList(declarationList);

            delete declarationList;
        }
    }

    return NULL;
}

void CParser::AfterReduce(int productionId, void *context)
{
    _stack[_stack.size() - 1].Object = context;
    _stack[_stack.size() - 1].Value = _value;

#ifdef CPARSER_DEBUG_OUTPUT
    std::cerr << "reduce " << _generator->GetProduction(productionId)->Lhs->Text << std::endl;
    std::string s;
    GetStackString(s);
    std::cerr << s << std::endl;
#endif
}

void CParser::Accept()
{
#ifdef CPARSER_DEBUG_OUTPUT
    std::cerr << "accept" << std::endl;
#endif
}

void CParser::SetError(std::string &errorMessage)
{
    _error = true;
    _errorMessage = errorMessage;
}

void CParser::SetError(const char *errorMessage)
{
    _error = true;
    _errorMessage = errorMessage;
}

int CParser::TokenToSymbolId(Token &token)
{
    if (token.Type == IdType)
    {
        return StringToSymbolId("ID");
    }
    else if (token.Type == LiteralNumberType)
    {
        return StringToSymbolId("LITERAL_NUMBER");
    }
    else if (token.Type == LiteralCharType)
    {
        return StringToSymbolId("LITERAL_CHAR");
    }
    else if (token.Type == LiteralStringType)
    {
        return StringToSymbolId("LITERAL_STRING");
    }
    else if (token.Type > LowerWordType && token.Type < UpperWordType)
    {
        std::string s = token.Text;

        for (size_t i = 0; i < s.length(); i++)
            s[i] = (char)toupper(s[i]);

        return _generator->GetSymbol(s)->Id;
    }
    else if ((token.Type > LowerSymType && token.Type < UpperSymType) ||
        (token.Type > LowerOpType && token.Type < UpperOpType))
    {
        std::string s  = token.Text;

        s.insert(s.begin(), '"');
        s.insert(s.end(), '"');

        return _generator->GetSymbol(s)->Id;
    }
    else
    {
        return -1;
    }
}

int CParser::StringToSymbolId(const char *string)
{
    std::string s = string;

    return _generator->GetSymbol(s)->Id;
}

void CParser::GetStackString(std::string &appendTo)
{
    for (size_t i = 0; i < _stack.size(); i++)
    {
        appendTo += _generator->GetSymbol(_stack[i].LrItem.Symbol)->Text;

        if (i != _stack.size() - 1)
            appendTo += ' ';
    }
}

IcImmediateValue CParser::CreateImmediateNumber(std::string &text, IcType *type)
{
    std::string temp = text;
    bool uSuffix = false;
    bool lSuffix = false;
    bool llSuffix = false;
    unsigned long long n = 0;

    while (text.length() != 0)
    {
        if (text[text.length() - 1] == 'u' || text[text.length() - 1] == 'U')
        {
            uSuffix = true;
            temp.resize(text.length() - 1);
        }
        else if (text[text.length() - 1] == 'l' || text[text.length() - 1] == 'L')
        {
            if (lSuffix)
                llSuffix = true;
            else
                lSuffix = true;

            temp.resize(text.length() - 1);
        }
        else
        {
            break;
        }
    }

    if (text.length() != 0 && text[0] == '0')
    {
        if (text.length() > 1 && text[1] == 'x')
        {
            for (size_t i = 2; i < text.length(); i++)
            {
                n *= 16;

                if (text[i] >= '0' && text[i] <= '9')
                {
                    n += text[i] - '0';
                }
                else if (text[i] >= 'a' && text[i] <= 'f')
                {
                    n += text[i] - 'a' + 10;
                }
                else if (text[i] >= 'A' && text[i] <= 'F')
                {
                    n += text[i] - 'A' + 10;
                }
                else
                {
                    throw "invalid character in hexadecimal literal";
                }
            }
        }
        else
        {
            for (size_t i = 1; i < text.length(); i++)
            {
                n *= 8;

                if (text[i] >= '0' && text[i] <= '7')
                {
                    n += text[i] - '0';
                }
                else
                {
                    throw "invalid character in octal literal";
                }
            }
        }
    }
    else
    {
        for (size_t i = 0; i < text.length(); i++)
        {
            n *= 10;

            if (text[i] >= '0' && text[i] <= '9')
            {
                n += text[i] - '0';
            }
            else
            {
                throw "invalid character in decimal literal";
            }
        }
    }

    IcImmediateValue value;

    memset(&value, 0, sizeof(IcImmediateValue));

    if (type)
    {
        memset(type, 0, sizeof(IcType));
        type->Kind = IcBaseTypeKind;
    }

    if (n <= UINT_MAX)
    {
        value.Type = IcIntType;
        value.u.Int = (int)n;

        if (type)
        {
            type->Base.Type = IcIntType;

            if (uSuffix || n > INT_MAX)
                type->Traits = IcUnsignedTrait;
        }
    }
    else if (n <= ULONG_MAX)
    {
        value.Type = IcLongType;
        value.u.Long = (long)n;

        if (type)
        {
            type->Base.Type = IcLongType;

            if (uSuffix || n > LONG_MAX)
                type->Traits = IcUnsignedTrait;
        }
    }
    else if (n <= ULLONG_MAX)
    {
        value.Type = IcLongLongType;
        throw "long long literals not supported";

        if (type)
        {
            type->Base.Type = IcLongLongType;

            if (uSuffix || n > LLONG_MAX)
                type->Traits = IcUnsignedTrait;
        }
    }

    return value;
}

IcImmediateValue CParser::CreateImmediateNumber(int value, IcBaseType type)
{
    IcImmediateValue v;

    memset(&v, 0, sizeof(IcImmediateValue));
    v.Type = type;

    switch (type)
    {
    case IcCharType:
        v.u.Char = (char)value;
        break;
    case IcShortType:
        v.u.Short = (short)value;
        break;
    case IcIntType:
        v.u.Int = value;
        break;
    case IcLongType:
        v.u.Long = (long)value;
        break;
    case IcLongLongType:
        throw "long long not supported";
        break;
    case IcIntPtrType:
        v.u.IntPtr = value;
        break;
    default:
        throw "invalid type";
    }

    return v;
}

IcImmediateValue CParser::CreateImmediateChar(std::string &text)
{
    size_t i = 0;
    bool wide = false;

    if (text[0] == 'L')
    {
        wide = true;
        i++;
    }

    if (text.length() < 2 + i || text[i] != '\'' || text[text.length() - 1] != '\'')
        throw "malformed char literal";

    i++;

    IcImmediateValue value;

    memset(&value, 0, sizeof(IcImmediateValue));

    if (wide)
    {
        std::wstring result;

        UnescapeStringWide(result, text.begin() + i, text.end() - 1);

        if (result.length() == 1)
        {
            value.Type = IcShortType;
            value.u.Short = result[0];
        }
        else if (result.length() == 0)
        {
            throw "empty char literal";
        }
        else
        {
            throw "char literal too long";
        }
    }
    else
    {
        std::string result;

        UnescapeString(result, text.begin() + i, text.end() - 1);

        if (result.length() == 1)
        {
            value.Type = IcCharType;
            value.u.Char = result[0];
        }
        else if (result.length() == 0)
        {
            throw "empty char literal";
        }
        else
        {
            throw "char literal too long";
        }
    }

    return value;
}

IcImmediateValue CParser::CreateImmediateString(std::string &text, IcType *type)
{
    size_t i = 0;
    bool wide = false;

    if (text[0] == 'L')
    {
        wide = true;
        i++;
    }

    if (text.length() < 2 + i || text[i] != '"' || text[text.length() - 1] != '"')
        throw "malformed string literal";

    i++;

    IcImmediateValue value;

    memset(&value, 0, sizeof(IcImmediateValue));

    if (wide)
    {
        std::wstring result;

        UnescapeStringWide(result, text.begin() + i, text.end() - 1);

        memset(type, 0, sizeof(IcType));
        type->Kind = IcArrayTypeKind;
        type->Array.Size = result.length() + 1;
        type->Array.Child = new IcType;
        memset(type->Array.Child, 0, sizeof(IcType));
        type->Array.Child->Kind = IcBaseTypeKind;
        type->Array.Child->Base.Type = IcShortType;

        short *data = (short *)malloc((result.length() + 1) * 2);

        memcpy(data, result.data(), result.length() * 2);
        data[result.length()] = 0;

        IcObject *object = _objectManager->CreateData(NULL, (char *)data, (int)((result.length() + 1) * 2));

        value.Type = IcReferenceType;
        value.u.Reference = object->Id;
    }
    else
    {
        std::string result;

        UnescapeString(result, text.begin() + i, text.end() - 1);

        memset(type, 0, sizeof(IcType));
        type->Kind = IcArrayTypeKind;
        type->Array.Size = result.length() + 1;
        type->Array.Child = new IcType;
        memset(type->Array.Child, 0, sizeof(IcType));
        type->Array.Child->Kind = IcBaseTypeKind;
        type->Array.Child->Base.Type = IcCharType;

        char *data = (char *)malloc(result.length() + 1);

        memcpy(data, result.data(), result.length());
        data[result.length()] = 0;

        IcObject *object = _objectManager->CreateData(NULL, data, (int)(result.length() + 1));

        value.Type = IcReferenceType;
        value.u.Reference = object->Id;
    }

    return value;
}

void CParser::UnescapeString(std::string &output, std::string::const_iterator begin, std::string::const_iterator end)
{
    while (begin != end)
    {
        if (*begin == '\\')
        {
            ++begin;

            if (begin == end)
                throw "invalid escape sequence";

            char c = ConvertEscapeCharacter(*begin);

            if (c == 0)
            {
                int r;

                if (!ConvertEscapeNumber(begin, end, r))
                    throw "unrecognized escape sequence";
                if (r > UCHAR_MAX)
                    throw "character too large for char";

                output.push_back((char)r);
            }
            else
            {
                output.push_back(c);
                ++begin;
            }
        }
        else
        {
            output.push_back(*begin);
            ++begin;
        }
    }
}

void CParser::UnescapeStringWide(std::wstring &output, std::string::const_iterator begin, std::string::const_iterator end)
{
    while (begin != end)
    {
        if (*begin == '\\')
        {
            ++begin;

            if (begin == end)
                throw "invalid escape sequence";

            char c = ConvertEscapeCharacter(*begin);

            if (c == 0)
            {
                int r;

                if (!ConvertEscapeNumber(begin, end, r))
                    throw "unrecognized escape sequence";
                if (r > USHRT_MAX)
                    throw "character too large for wchar";

                output.push_back((short)r);
            }
            else
            {
                output.push_back((unsigned char)c);
                ++begin;
            }
        }
        else
        {
            output.push_back((unsigned char)*begin);
            ++begin;
        }
    }
}

char CParser::ConvertEscapeCharacter(char escape)
{
    switch (escape)
    {
    case 'a':
        return '\a';
    case 'b':
        return '\b';
    case 'f':
        return '\f';
    case 'n':
        return '\n';
    case 'r':
        return '\r';
    case 't':
        return '\t';
    case 'v':
        return '\v';
    case '\'':
        return '\'';
    case '"':
        return '"';
    case '\\':
        return '\\';
    case '?':
        return '?';
    default:
        return 0;
    }
}

bool CParser::ConvertEscapeNumber(std::string::const_iterator &current, std::string::const_iterator end, int &result)
{
    result = 0;

    if (*current >= '0' && *current <= '7')
    {
        result = *current - '0';
        ++current;

        while (current != end)
        {
            if (*current >= '0' && *current <= '7')
            {
                result *= 8;
                result += *current - '0';
            }
            else
            {
                break;
            }

            ++current;
        }
    }
    else if (*current == 'x')
    {
        ++current;

        if (current == end)
            throw "hexadecimal character must have at least one digit";

        do
        {
            if (*current >= '0' && *current <= '9')
            {
                result *= 16;
                result += *current - '0';
            }
            else if (*current >= 'a' && *current <= 'f')
            {
                result *= 16;
                result += *current - 'a' + 10;
            }
            else if (*current >= 'A' && *current <= 'F')
            {
                result *= 16;
                result += *current - 'A' + 10;
            }
            else
            {
                break;
            }

            ++current;
        } while (current != end);
    }
    else
    {
        return false;
    }

    return true;
}

IcType CParser::BasicTypeToIcType(CBasicType basicType)
{
    IcType type;

    type.Kind = IcBaseTypeKind;
    type.Traits = 0;
    type.Base.EnumTag = NULL;

    switch ((unsigned int)basicType & CBasicTypeMask)
    {
    case CVoidType:
        type.Base.Type = IcVoidType;
        break;
    case CCharType:
        type.Base.Type = IcCharType;
        break;
    case CShortType:
        type.Base.Type = IcShortType;
        break;
    case CIntType:
        type.Base.Type = IcIntType;
        break;
    case CLongType:
        type.Base.Type = IcLongType;
        break;
    case CFloatType:
        type.Base.Type = IcFloatType;
        break;
    case CDoubleType:
        type.Base.Type = IcDoubleType;
        break;
    }

    if (basicType & CUnsignedType)
        type.Traits |= IcUnsignedTrait;

    return type;
}

IcType CParser::CreateIcType(IcType &type, CDeclarator &declarator, bool parameterNames)
{
    IcType ic;
    IcType *childIc;

    ic = type;

    if (ic.Kind == IcEnumTypeKind)
    {
        std::string *tag = &ic.Enum->Tag;

        ic.Kind = IcBaseTypeKind;
        ic.Base.Type = IcIntType;
        ic.Base.EnumTag = tag;
    }

    for (auto it = declarator.Chain.begin(); it != declarator.Chain.end(); ++it)
    {
        childIc = new IcType(ic);
        ic.Kind = IcPointerTypeKind;
        ic.Traits = 0;
        ic.Pointer.Child = childIc;

        CTypeQualifier pointerQualifier = *it;

        if ((unsigned int)pointerQualifier & CConstQualifier)
            ic.Traits |= IcConstTrait;
        if ((unsigned int)pointerQualifier & CVolatileQualifier)
            ic.Traits |= IcVolatileTrait;
    }

    for (auto it = declarator.Dimensions.begin(); it != declarator.Dimensions.end(); ++it)
    {
        childIc = new IcType(ic);
        ic.Kind = IcArrayTypeKind;
        ic.Traits = 0;
        ic.Array.Child = childIc;
        ic.Array.Size = *it;
    }

    if (declarator.Function)
    {
        IcType newIc;

        if (ic.Kind == IcArrayTypeKind)
            throw "function returns array (illegal return type)";

        newIc.Kind = IcFunctionTypeKind;
        newIc.Traits = 0;
        newIc.Function = new IcFunctionType;
        newIc.Function->ReturnType = ic;
        newIc.Function->Variadic = declarator.Parameters.Ellipsis;

        if (parameterNames)
            newIc.Function->ParameterNames = new std::vector<std::string>;
        else
            newIc.Function->ParameterNames = NULL;

        for (auto it = declarator.Parameters.List.begin(); it != declarator.Parameters.List.end(); ++it)
        {
            IcType parameterType = CreateIcType(it->Type, it->Qualifiers, it->Declarator, it->TypeSpecified);

            if (parameterType.Kind == IcArrayTypeKind)
            {
                IcType *subType = parameterType.Array.Child;

                parameterType.Kind = IcPointerTypeKind;
                parameterType.Traits = 0;
                parameterType.Pointer.Child = subType;
            }

            newIc.Function->Parameters.push_back(parameterType);

            if (newIc.Function->ParameterNames)
                newIc.Function->ParameterNames->push_back(it->Declarator.FindName());
        }

        if (declarator.Child && (declarator.Child->Chain.size() != 0 || declarator.Child->Dimensions.size() != 0 || declarator.Child->Function))
        {
            return CreateIcType(newIc, *declarator.Child);
        }

        return newIc;
    }

    return ic;
}

IcType CParser::CreateIcType(CTypeSpecifier &type, CTypeQualifier qualifier, CDeclarator &declarator, bool typeSpecified, bool parameterNames)
{
    IcType ic;

    if (!typeSpecified)
        throw "no type specified (default-int not allowed)";

    if (type.Type == CBasicTypeSpecifier)
        ic = BasicTypeToIcType(type.Basic);
    else
        ic = type.Ic;

    if ((unsigned int)qualifier & CConstQualifier)
        ic.Traits |= IcConstTrait;
    if ((unsigned int)qualifier & CVolatileQualifier)
        ic.Traits |= IcVolatileTrait;

    return CreateIcType(ic, declarator, parameterNames);
}

IcRegister CParser::AllocateRegister()
{
    return _nextRegister++;
}

IcVariableId CParser::AllocateVariableId()
{
    return _nextVariableId++;
}

void CParser::AppendOp(IcOperation &op)
{
    if (_currentBlock)
        _currentBlock->Operations.push_back(op);
    _evaluator.Execute(op);
}

void CParser::PushControlScope(IcControlKeyword keyword)
{
    IcControlScope *controlScope = new IcControlScope(keyword);

    controlScope->Parent = _controlScope;
    _controlScope = controlScope;
}

void CParser::PopControlScope()
{
    IcControlScope *controlScope = _controlScope;

    _controlScope = _controlScope->Parent;
    delete controlScope;
}

IcScope *CParser::GetScopeByDepth(int depth)
{
    IcScope *scope = _scope;

    depth = _scopeDepth - depth;

    while (scope && depth != 0)
    {
        scope = scope->Parent;
        depth--;
    }

    return scope;
}

bool CParser::SideEffectsDisabled()
{
    return _disableSideEffects > 0;
}

void CParser::DisableSideEffects()
{
    _disableSideEffects++;
}

void CParser::EnableSideEffects()
{
    _disableSideEffects--;
}

bool CParser::CheckType(IcType &type, unsigned int options)
{
    if (type.Kind == IcBaseTypeKind)
    {
        if (options & CAllowInt)
        {
            switch (type.Base.Type)
            {
            case IcCharType:
            case IcShortType:
            case IcIntType:
            case IcLongType:
            case IcLongLongType:
                return true;
            }
        }

        if (options & CAllowFloat)
        {
            switch (type.Base.Type)
            {
            case IcFloatType:
            case IcDoubleType:
                return true;
            }
        }

        if (options & CAllowVoid)
        {
            if (type.Base.Type == IcVoidType)
                return true;
        }
    }
    else if (type.Kind == IcPointerTypeKind)
    {
        if (options & CAllowPointer)
            return true;
    }
    else if (type.Kind == IcArrayTypeKind)
    {
        if ((options & CAllowArray) && type.Array.Size != -1)
            return true;
    }
    else if (type.Kind == IcStructTypeKind)
    {
        if (options & CAllowStruct)
            return true;
    }

    return false;
}

bool CParser::CheckCompatiblePointerTypes(IcType &type1, IcType &type2, bool allowVoidPointer)
{
    assert(type1.Kind == IcPointerTypeKind);
    assert(type2.Kind == IcPointerTypeKind);

    if (allowVoidPointer && (
        (type1.Pointer.Child->Kind == IcBaseTypeKind && type1.Pointer.Child->Base.Type == IcVoidType) ||
        (type2.Pointer.Child->Kind == IcBaseTypeKind && type2.Pointer.Child->Base.Type == IcVoidType)))
    {
        return true;
    }
    else
    {
        return GetChildForPointerType(type1).EqualTo(GetChildForPointerType(type2));
    }
}

IcBaseType CParser::GetBaseType(IcType &type)
{
    if (type.Kind == IcBaseTypeKind)
        return type.Base.Type;
    else if (type.Kind == IcPointerTypeKind)
        return IcIntPtrType;

    throw "invalid type";
}

IcType CParser::GetChildForPointerType(IcType &type)
{
    assert(type.Kind == IcPointerTypeKind);

    if (type.Pointer.Child->Kind == IcIncompleteTypeKind)
    {
        IcType &child = *type.Pointer.Child;
        IcScope *scope = GetScopeByDepth(child.Incomplete.ScopeDepth);
        IcTagSymbol *tagSymbol;

        if (scope && (tagSymbol = scope->LookupTag(*child.Incomplete.Tag)) && tagSymbol->Type.Kind == IcStructTypeKind)
        {
            if (tagSymbol->Type.Struct->Keyword == child.Incomplete.Keyword)
                return tagSymbol->Type;
        }

        return *type.Pointer.Child;
    }
    else
    {
        return *type.Pointer.Child;
    }
}

IcOpcode CParser::OpcodeForSourceType(IcType &type)
{
    if (type.Kind == IcBaseTypeKind)
    {
        switch (type.Base.Type)
        {
        case IcCharType:
            if (type.Traits & IcUnsignedTrait)
                return IcOpCvtUC;
            else
                return IcOpCvtC;
        case IcShortType:
            if (type.Traits & IcUnsignedTrait)
                return IcOpCvtUS;
            else
                return IcOpCvtS;
        case IcIntType:
            if (type.Traits & IcUnsignedTrait)
                return IcOpCvtUI;
            else
                return IcOpCvtI;
        case IcLongType:
            if (type.Traits & IcUnsignedTrait)
                return IcOpCvtUL;
            else
                return IcOpCvtL;
        case IcLongLongType:
            if (type.Traits & IcUnsignedTrait)
                return IcOpCvtULL;
            else
                return IcOpCvtLL;
        case IcFloatType:
            return IcOpCvtF;
        case IcDoubleType:
            return IcOpCvtD;
        default:
            throw "invalid type";
        }
    }
    else if (type.Kind == IcPointerTypeKind)
    {
        return IcOpCvtP;
    }
    else
    {
        throw "invalid type";
    }
}

bool CParser::IsConstantZero(IcStorage &storage)
{
    IcImmediateValue value;

    if (storage.Kind != IcRegisterStorageKind)
        return false;
    if (!_evaluator.GetRegister(storage.r.Register, value))
        return false;

    return IsConstantZero(value);
}

bool CParser::IsConstantZero(IcImmediateValue &value)
{
    switch (value.Type)
    {
    case IcCharType:
        return value.u.Char == 0;
    case IcShortType:
        return value.u.Short == 0;
    case IcIntType:
        return value.u.Int == 0;
    case IcLongType:
        return value.u.Long == 0;
    case IcIntPtrType:
        return value.u.IntPtr == 0;
    default:
        throw "invalid type";
    }
}

void CParser::ConvertOneType(IcTypedValue &value, unsigned int options, IcTypedValue &newValue)
{
    if (!(options & CDontConvertArray) && value.Type.Kind == IcArrayTypeKind)
    {
        assert(value.Value.Kind == IcIndirectStorageKind);
        memset(&newValue, 0, sizeof(IcTypedValue));
        newValue.Type.Kind = IcPointerTypeKind;
        newValue.Type.Traits = 0;
        newValue.Type.Pointer.Child = new IcType(*value.Type.Array.Child);
        newValue.Value.Kind = IcRegisterStorageKind;
        newValue.Value.r.Register = value.Value.i.Address;

        return;
    }

    newValue = value;
}

void CParser::CommonNumberType(IcType &type1, IcType &type2, IcType &newType)
{
    assert(type1.Kind == IcBaseTypeKind);
    assert(type2.Kind == IcBaseTypeKind);

    memset(&newType, 0, sizeof(IcType));
    newType.Kind = IcBaseTypeKind;

    unsigned int unsigned1 = type1.Traits & IcUnsignedTrait;
    unsigned int unsigned2 = type2.Traits & IcUnsignedTrait;

    if (type1.Base.Type == IcDoubleType || type2.Base.Type == IcDoubleType)
    {
        newType.Base.Type = IcDoubleType;
    }
    else if (type1.Base.Type == IcFloatType || type2.Base.Type == IcFloatType)
    {
        newType.Base.Type = IcFloatType;
    }
    else if ((type1.Base.Type == type2.Base.Type) && (unsigned1 == unsigned2))
    {
        newType.Base.Type = type1.Base.Type;
        newType.Traits = unsigned1;
    }
    else if (unsigned1 == unsigned2)
    {
        newType.Base.Type = std::max(type1.Base.Type, type2.Base.Type);
        newType.Traits = unsigned1;
    }
    else if ((unsigned1 && (type1.Base.Type > type2.Base.Type)) ||
        (unsigned2 && (type2.Base.Type > type1.Base.Type)))
    {
        newType.Base.Type = std::max(type1.Base.Type, type2.Base.Type);
        newType.Traits = IcUnsignedTrait;
    }
    else if ((!unsigned1 && (type1.SizeOf() > type2.SizeOf())) ||
        (!unsigned2 && (type2.SizeOf() > type1.SizeOf())))
    {
        newType.Base.Type = !unsigned1 ? type1.Base.Type : type2.Base.Type;
    }
    else
    {
        newType.Base.Type = !unsigned1 ? type1.Base.Type : type2.Base.Type;
        newType.Traits = IcUnsignedTrait;
    }
}

IcRegister CParser::ReadValue(IcTypedValue &value, IcType *newType)
{
    IcRegister r;
    IcBaseType sourceType;

    sourceType = GetBaseType(value.Type);

    if (value.Value.Kind == IcRegisterStorageKind)
    {
        r = value.Value.r.Register;
    }
    else
    {
        IcOperation op;

        r = AllocateRegister();

        op.Code = IcOpLd;
        op.Mode = sourceType;
        op.rr.R1 = value.Value.i.Address;
        op.rr.R2 = r;
        AppendOp(op);
    }

    if (newType)
    {
        IcBaseType destinationType;

        destinationType = GetBaseType(*newType);

        if (destinationType != sourceType)
        {
            IcOperation op;

            op.Code = OpcodeForSourceType(value.Type);
            op.Mode = destinationType;
            op.rr.R1 = r;
            op.rr.R2 = AllocateRegister();
            AppendOp(op);

            return op.rr.R2;
        }
    }

    return r;
}

IcTypedValue CParser::LoadInt(int constant, IcBaseType type)
{
    IcTypedValue value;
    IcOperation op;

    memset(&value, 0, sizeof(IcTypedValue));
    value.Type.Kind = IcBaseTypeKind;
    value.Type.Base.Type = type;
    value.Value.Kind = IcRegisterStorageKind;
    value.Value.r.Register = AllocateRegister();

    op.Code = IcOpLdI;
    op.Mode = type;
    op.cr.C1 = CreateImmediateNumber(constant, type);
    op.cr.R2 = value.Value.r.Register;
    AppendOp(op);

    return value;
}

IcStorage CParser::AllocateVariable(IcType &type, IcVariableId *variableId)
{
    IcOperation op;
    IcStorage storage;
    IcVariableId id;
    IcFunctionVariable functionVariable;

    id = AllocateVariableId();

    if (CheckType(type, CAllowInt | CAllowFloat | CAllowPointer | CAllowArray | CAllowStruct))
    {
        int size = type.SizeOf();

        if (size == -1)
            throw "cannot calculate size of type";

        int slotId = _functionScope->AllocateSlot(size, id);

        memset(&storage, 0, sizeof(IcStorage));
        storage.Kind = IcIndirectStorageKind;
        storage.i.Address = AllocateRegister();
        storage.i.LValue = true;

        op.Code = IcOpLdF;
        op.Mode = IcIntPtrType;
        op.cr.C1 = CreateImmediateNumber(slotId, IcIntType);
        op.cr.R2 = storage.i.Address;
        AppendOp(op);

        functionVariable.Parameter = false;
        functionVariable.Index = slotId;
        functionVariable.AddressRegister = storage.i.Address;
        functionVariable.Type = type;
        _functionScope->Variables[id] = functionVariable;
    }
    else
    {
        throw "invalid type for local variable";
    }

    if (variableId)
        *variableId = id;

    return storage;
}

IcTypedValue CParser::GetVariable(IcVariableId variableId)
{
    IcTypedValue value;

    assert(_functionScope);
    assert(_functionScope->Variables.find(variableId) != _functionScope->Variables.end());

    value.Type = _functionScope->Variables[variableId].Type;
    memset(&value.Value, 0, sizeof(IcStorage));
    value.Value.Kind = IcIndirectStorageKind;
    value.Value.i.Address = _functionScope->Variables[variableId].AddressRegister;
    value.Value.i.LValue = true;

    return value;
}

void CParser::SequencePoint()
{
    for (auto it = _pendingSideEffects.begin(); it != _pendingSideEffects.end(); ++it)
    {
        IcTypedValue newValue;
        IcTypedValue oneValue;
        IcTypedValue dummy;

        switch (it->Type)
        {
        case CPendingIncrementEffect:
            oneValue = LoadInt(1, IcIntType);
            AddExpressionAdd(it->Value, oneValue, newValue);
            AssignExpressionAssign(it->Value, newValue, dummy);
            break;
        case CPendingDecrementEffect:
            oneValue = LoadInt(1, IcIntType);
            AddExpressionSubtract(it->Value, oneValue, newValue);
            AssignExpressionAssign(it->Value, newValue, dummy);
            break;
        }
    }

    _pendingSideEffects.clear();
}

void CParser::AssignExpressionAssign(IcTypedValue &input1, IcTypedValue &input2, IcTypedValue &newValue, bool returnValue)
{
    IcOperation op;
    IcTypedValue value1;
    IcTypedValue value2;
    bool isStruct = false;
    bool nullPointer = false;

    memset(&newValue, 0, sizeof(IcTypedValue));

    ConvertOneType(input1, 0, value1);
    ConvertOneType(input2, 0, value2);

    if (CheckType(value1.Type, CAllowInt | CAllowFloat) && CheckType(value2.Type, CAllowInt | CAllowFloat))
    {
        // Nothing
    }
    else if (CheckType(value1.Type, CAllowStruct) && CheckType(value2.Type, CAllowStruct))
    {
        if (!value1.Type.EqualTo(value2.Type))
            throw returnValue ? "type mismatch in return value" : "type mismatch in assignment";

        isStruct = true;
    }
    else if (CheckType(value1.Type, CAllowPointer) && CheckType(value2.Type, CAllowPointer))
    {
        if (!CheckCompatiblePointerTypes(value1.Type, value2.Type, true))
            throw returnValue ? "pointer type mismatch in return value" : "pointer type mismatch in assignment";
    }
    else if (CheckType(value1.Type, CAllowPointer) && CheckType(value2.Type, CAllowInt) && IsConstantZero(value2.Value))
    {
        nullPointer = true;
    }
    else
    {
        throw returnValue ? "type mismatch in return value" : "type mismatch in assignment";
    }

    if (!isStruct)
    {
        if (value1.Value.Kind != IcIndirectStorageKind || !value1.Value.i.LValue)
        {
            throw "left side of assignment is not an l-value";
        }

        newValue.Type = value1.Type;
        newValue.Value.Kind = IcRegisterStorageKind;

        if (_currentBlock)
        {
            if (nullPointer)
            {
                newValue.Value.r.Register = AllocateRegister();

                op.Code = IcOpLdI;
                op.Mode = GetBaseType(value1.Type);
                op.cr.C1 = CreateImmediateNumber(0, op.Mode);
                op.cr.R2 = newValue.Value.r.Register;
                AppendOp(op);
            }
            else
            {
                newValue.Value.r.Register = ReadValue(value2, &value1.Type);
            }
        }
        else
        {
            // Force non-constant value.
            newValue.Value.r.Register = AllocateRegister();
        }

        op.Code = IcOpSt;
        op.Mode = GetBaseType(value1.Type);
        op.rr.R1 = newValue.Value.r.Register;
        op.rr.R2 = value1.Value.i.Address;
        AppendOp(op);
    }
    else
    {
        if (!value1.Value.i.LValue)
            throw "left side of assignment is not an l-value";

        assert(value2.Value.Kind == IcIndirectStorageKind);

        int size = value1.Type.SizeOf();

        op.Code = IcOpCpyI;
        op.Mode = IcInvalidType;
        op.rrc.R1 = value2.Value.i.Address;
        op.rrc.R2 = value1.Value.i.Address;
        op.rrc.C3 = CreateImmediateNumber(size, IcIntType);
        AppendOp(op);

        newValue = value2;
    }
}

void CParser::OrExpression(IcTypedValue &input1, IcTypedValue &input2, IcTypedValue &newValue)
{
    IcOperation op;
    IcTypedValue value1;
    IcTypedValue value2;
    IcType commonType;

    ConvertOneType(input1, 0, value1);
    ConvertOneType(input2, 0, value2);

    if (!CheckType(value1.Type, CAllowInt) || !CheckType(value2.Type, CAllowInt))
        throw "invalid type for '|' operator";

    CommonNumberType(value1.Type, value2.Type, commonType);

    memset(&newValue, 0, sizeof(IcTypedValue));
    newValue.Type = commonType;
    newValue.Value.Kind = IcRegisterStorageKind;
    newValue.Value.r.Register = AllocateRegister();

    op.Code = IcOpOr;
    op.Mode = commonType.Base.Type;
    op.rrr.R1 = ReadValue(value1, &commonType);
    op.rrr.R2 = ReadValue(value2, &commonType);
    op.rrr.R3 = newValue.Value.r.Register;
    AppendOp(op);
}

void CParser::XorExpression(IcTypedValue &input1, IcTypedValue &input2, IcTypedValue &newValue)
{
    IcOperation op;
    IcTypedValue value1;
    IcTypedValue value2;
    IcType commonType;

    ConvertOneType(input1, 0, value1);
    ConvertOneType(input2, 0, value2);

    if (!CheckType(value1.Type, CAllowInt) || !CheckType(value2.Type, CAllowInt))
        throw "invalid type for '^' operator";

    CommonNumberType(value1.Type, value2.Type, commonType);

    memset(&newValue, 0, sizeof(IcTypedValue));
    newValue.Type = commonType;
    newValue.Value.Kind = IcRegisterStorageKind;
    newValue.Value.r.Register = AllocateRegister();

    op.Code = IcOpXor;
    op.Mode = commonType.Base.Type;
    op.rrr.R1 = ReadValue(value1, &commonType);
    op.rrr.R2 = ReadValue(value2, &commonType);
    op.rrr.R3 = newValue.Value.r.Register;
    AppendOp(op);
}

void CParser::AndExpression(IcTypedValue &input1, IcTypedValue &input2, IcTypedValue &newValue)
{
    IcOperation op;
    IcTypedValue value1;
    IcTypedValue value2;
    IcType commonType;

    ConvertOneType(input1, 0, value1);
    ConvertOneType(input2, 0, value2);

    if (!CheckType(value1.Type, CAllowInt) || !CheckType(value2.Type, CAllowInt))
        throw "invalid type for '&' operator";

    CommonNumberType(value1.Type, value2.Type, commonType);

    memset(&newValue, 0, sizeof(IcTypedValue));
    newValue.Type = commonType;
    newValue.Value.Kind = IcRegisterStorageKind;
    newValue.Value.r.Register = AllocateRegister();

    op.Code = IcOpAnd;
    op.Mode = commonType.Base.Type;
    op.rrr.R1 = ReadValue(value1, &commonType);
    op.rrr.R2 = ReadValue(value2, &commonType);
    op.rrr.R3 = newValue.Value.r.Register;
    AppendOp(op);
}

void CParser::EqualityExpression(std::string &tag, IcTypedValue &input1, IcTypedValue &input2, IcTypedValue &newValue)
{
    IcOperation op;
    IcTypedValue value1;
    IcTypedValue value2;
    IcType commonType;
    IcTypedValue *pointerValue;
    bool nullPointer = false;

    ConvertOneType(input1, 0, value1);
    ConvertOneType(input2, 0, value2);

    if (CheckType(value1.Type, CAllowInt | CAllowFloat) && CheckType(value2.Type, CAllowInt | CAllowFloat))
    {
        CommonNumberType(value1.Type, value2.Type, commonType);
    }
    else if (CheckType(value1.Type, CAllowPointer) && CheckType(value2.Type, CAllowPointer))
    {
        if (!CheckCompatiblePointerTypes(value1.Type, value2.Type, true))
            throw "incompatible pointer types";

        commonType = value1.Type;
    }
    else if ((CheckType(value1.Type, CAllowPointer) && CheckType(value2.Type, CAllowInt) && IsConstantZero(value2.Value)) ||
        (CheckType(value1.Type, CAllowInt) && IsConstantZero(value1.Value) && CheckType(value2.Type, CAllowPointer)))
    {
        pointerValue = value1.Type.Kind == IcPointerTypeKind ? &value1 : &value2;
        nullPointer = true;
    }
    else
    {
        throw "invalid type for equality operator";
    }

    memset(&newValue, 0, sizeof(IcTypedValue));
    newValue.Type.Kind = IcBaseTypeKind;
    newValue.Type.Base.Type = IcIntType;
    newValue.Value.Kind = IcRegisterStorageKind;
    newValue.Value.r.Register = AllocateRegister();

    if (nullPointer)
    {
        op.Code = IcOpChEI;
        op.Mode = IcIntPtrType;
        op.rcccr.R1 = ReadValue(*pointerValue);
        op.rcccr.C2 = CreateImmediateNumber(0, IcIntPtrType);
        op.rcccr.R5 = newValue.Value.r.Register;

        if (tag == "Equal")
        {
            op.rcccr.C3 = CreateImmediateNumber(1, IcIntPtrType);
            op.rcccr.C4 = CreateImmediateNumber(0, IcIntPtrType);
        }
        else if (tag == "NotEqual")
        {
            op.rcccr.C3 = CreateImmediateNumber(0, IcIntPtrType);
            op.rcccr.C4 = CreateImmediateNumber(1, IcIntPtrType);
        }
    }
    else
    {
        op.Code = IcOpChE;
        op.Mode = GetBaseType(commonType);
        op.rrccr.R1 = ReadValue(value1, &commonType);
        op.rrccr.R2 = ReadValue(value2, &commonType);
        op.rrccr.R5 = newValue.Value.r.Register;

        if (tag == "Equal")
        {
            op.rrccr.C3 = CreateImmediateNumber(1, IcIntPtrType);
            op.rrccr.C4 = CreateImmediateNumber(0, IcIntPtrType);
        }
        else if (tag == "NotEqual")
        {
            op.rrccr.C3 = CreateImmediateNumber(0, IcIntPtrType);
            op.rrccr.C4 = CreateImmediateNumber(1, IcIntPtrType);
        }
    }

    AppendOp(op);
}

void CParser::RelationalExpression(std::string &tag, IcTypedValue &input1, IcTypedValue &input2, IcTypedValue &newValue)
{
    IcOperation op;
    IcTypedValue value1;
    IcTypedValue value2;
    IcType commonType;
    bool unsignedType;

    ConvertOneType(input1, 0, value1);
    ConvertOneType(input2, 0, value2);

    if (CheckType(value1.Type, CAllowInt | CAllowFloat) && CheckType(value2.Type, CAllowInt | CAllowFloat))
    {
        CommonNumberType(value1.Type, value2.Type, commonType);
        unsignedType = CheckType(commonType, CAllowInt) && (commonType.Traits & IcUnsignedTrait);
    }
    else if (CheckType(value1.Type, CAllowPointer) && CheckType(value2.Type, CAllowPointer))
    {
        if (!CheckCompatiblePointerTypes(value1.Type, value2.Type, false))
            throw "incompatible pointer types";

        commonType = value1.Type;
        unsignedType = true;
    }
    else
    {
        throw "invalid type for relational operator";
    }

    memset(&newValue, 0, sizeof(IcTypedValue));
    newValue.Type.Kind = IcBaseTypeKind;
    newValue.Type.Base.Type = IcIntType;
    newValue.Value.Kind = IcRegisterStorageKind;
    newValue.Value.r.Register = AllocateRegister();

    op.Mode = GetBaseType(commonType);
    op.rrccr.R1 = ReadValue(value1, &commonType);
    op.rrccr.R2 = ReadValue(value2, &commonType);
    op.rrccr.R5 = newValue.Value.r.Register;

    if (tag == "Less")
    {
        op.Code = unsignedType ? IcOpChB : IcOpChL;
        op.rrccr.C3 = CreateImmediateNumber(1, IcIntType);
        op.rrccr.C4 = CreateImmediateNumber(0, IcIntType);
    }
    else if (tag == "LessEqual")
    {
        op.Code = unsignedType ? IcOpChBE : IcOpChLE;
        op.rrccr.C3 = CreateImmediateNumber(1, IcIntType);
        op.rrccr.C4 = CreateImmediateNumber(0, IcIntType);
    }
    else if (tag == "Greater")
    {
        op.Code = unsignedType ? IcOpChBE : IcOpChLE;
        op.rrccr.C3 = CreateImmediateNumber(0, IcIntType);
        op.rrccr.C4 = CreateImmediateNumber(1, IcIntType);
    }
    else if (tag == "GreaterEqual")
    {
        op.Code = unsignedType ? IcOpChB : IcOpChL;
        op.rrccr.C3 = CreateImmediateNumber(0, IcIntType);
        op.rrccr.C4 = CreateImmediateNumber(1, IcIntType);
    }

    AppendOp(op);
}

void CParser::ShiftExpression(std::string &tag, IcTypedValue &input1, IcTypedValue &input2, IcTypedValue &newValue)
{
    IcOperation op;
    IcTypedValue value1;
    IcTypedValue value2;
    IcType commonType;

    ConvertOneType(input1, 0, value1);
    ConvertOneType(input2, 0, value2);

    if (!CheckType(value1.Type, CAllowInt) || !CheckType(value2.Type, CAllowInt))
        throw "invalid type for shift operator";

    CommonNumberType(value1.Type, value2.Type, commonType);

    memset(&newValue, 0, sizeof(IcTypedValue));
    newValue.Type = commonType;
    newValue.Value.Kind = IcRegisterStorageKind;
    newValue.Value.r.Register = AllocateRegister();

    if (tag == "LeftShift")
        op.Code = (commonType.Traits & IcUnsignedTrait) ? IcOpShlU : IcOpShl;
    else if (tag == "RightShift")
        op.Code = (commonType.Traits & IcUnsignedTrait) ? IcOpShrU : IcOpShr;

    op.Mode = commonType.Base.Type;
    op.rrr.R1 = ReadValue(value1, &commonType);
    op.rrr.R2 = ReadValue(value2, &commonType);
    op.rrr.R3 = newValue.Value.r.Register;
    AppendOp(op);
}

void CParser::AddExpressionAdd(IcTypedValue &input1, IcTypedValue &input2, IcTypedValue &newValue, bool pointerModeOnly, IcType *pointerType)
{
    IcOperation op;
    IcTypedValue value1;
    IcTypedValue value2;
    IcType commonType;

    memset(&newValue, 0, sizeof(IcTypedValue));

    if (pointerType)
        memset(pointerType, 0, sizeof(IcType));

    ConvertOneType(input1, 0, value1);
    ConvertOneType(input2, 0, value2);

    if (!pointerModeOnly && CheckType(value1.Type, CAllowInt | CAllowFloat) && CheckType(value2.Type, CAllowInt | CAllowFloat))
    {
        CommonNumberType(value1.Type, value2.Type, commonType);

        newValue.Type = commonType;
        newValue.Value.Kind = IcRegisterStorageKind;
        newValue.Value.r.Register = AllocateRegister();

        op.Code = IcOpAdd;
        op.Mode = commonType.Base.Type;
        op.rrr.R1 = ReadValue(value1, &commonType);
        op.rrr.R2 = ReadValue(value2, &commonType);
        op.rrr.R3 = newValue.Value.r.Register;
        AppendOp(op);
    }
    else if ((CheckType(value1.Type, CAllowPointer) && CheckType(value2.Type, CAllowInt)) ||
        (CheckType(value1.Type, CAllowInt) && CheckType(value2.Type, CAllowPointer)))
    {
        IcTypedValue &baseValue = value1.Type.Kind == IcPointerTypeKind ? value1 : value2;
        IcTypedValue &offsetValue = value1.Type.Kind == IcPointerTypeKind ? value2 : value1;
        int size = GetChildForPointerType(baseValue.Type).SizeOf();

        if (size == -1)
            throw "cannot calculate size of type";

        IcTypedValue byteOffsetValue;

        memset(&byteOffsetValue, 0, sizeof(IcTypedValue));
        byteOffsetValue.Type = offsetValue.Type;
        byteOffsetValue.Value.Kind = IcRegisterStorageKind;
        byteOffsetValue.Value.r.Register = AllocateRegister();

        op.Code = (offsetValue.Type.Traits & IcUnsignedTrait) ? IcOpMulUI : IcOpMulI;
        op.Mode = offsetValue.Type.Base.Type;
        op.rcr.R1 = ReadValue(offsetValue);
        op.rcr.C2 = CreateImmediateNumber(size, offsetValue.Type.Base.Type);
        op.rcr.R3 = byteOffsetValue.Value.r.Register;
        AppendOp(op);

        newValue.Type = baseValue.Type;
        newValue.Value.Kind = IcRegisterStorageKind;
        newValue.Value.r.Register = AllocateRegister();

        op.Code = IcOpAdd;
        op.Mode = IcIntPtrType;
        op.rrr.R1 = ReadValue(baseValue);
        op.rrr.R2 = ReadValue(byteOffsetValue, &baseValue.Type);
        op.rrr.R3 = newValue.Value.r.Register;
        AppendOp(op);

        if (pointerType)
            *pointerType = baseValue.Type;
    }
    else
    {
        throw "invalid type for '+', '++' or '[]' operator";
    }
}

void CParser::AddExpressionSubtract(IcTypedValue &input1, IcTypedValue &input2, IcTypedValue &newValue)
{
    IcOperation op;
    IcTypedValue value1;
    IcTypedValue value2;
    IcType commonType;

    memset(&newValue, 0, sizeof(IcTypedValue));

    ConvertOneType(input1, 0, value1);
    ConvertOneType(input2, 0, value2);

    if (CheckType(value1.Type, CAllowInt | CAllowFloat) && CheckType(value2.Type, CAllowInt | CAllowFloat))
    {
        CommonNumberType(value1.Type, value2.Type, commonType);

        newValue.Type = commonType;
        newValue.Value.Kind = IcRegisterStorageKind;
        newValue.Value.r.Register = AllocateRegister();

        op.Code = IcOpSub;
        op.Mode = commonType.Base.Type;
        op.rrr.R1 = ReadValue(value1, &commonType);
        op.rrr.R2 = ReadValue(value2, &commonType);
        op.rrr.R3 = newValue.Value.r.Register;
        AppendOp(op);
    }
    else if ((CheckType(value1.Type, CAllowPointer) && CheckType(value2.Type, CAllowInt)) ||
        (CheckType(value1.Type, CAllowInt) && CheckType(value2.Type, CAllowPointer)))
    {
        IcTypedValue &baseValue = value1.Type.Kind == IcPointerTypeKind ? value1 : value2;
        IcTypedValue &offsetValue = value1.Type.Kind == IcPointerTypeKind ? value2 : value1;
        int size = GetChildForPointerType(baseValue.Type).SizeOf();

        if (size == -1)
            throw "cannot calculate size of type";

        IcTypedValue byteOffsetValue;

        memset(&byteOffsetValue, 0, sizeof(IcTypedValue));
        byteOffsetValue.Type = offsetValue.Type;
        byteOffsetValue.Value.Kind = IcRegisterStorageKind;
        byteOffsetValue.Value.r.Register = AllocateRegister();

        op.Code = (offsetValue.Type.Traits & IcUnsignedTrait) ? IcOpMulUI : IcOpMulI;
        op.Mode = offsetValue.Type.Base.Type;
        op.rcr.R1 = ReadValue(offsetValue);
        op.rcr.C2 = CreateImmediateNumber(size, offsetValue.Type.Base.Type);
        op.rcr.R3 = byteOffsetValue.Value.r.Register;
        AppendOp(op);

        newValue.Type = baseValue.Type;
        newValue.Value.Kind = IcRegisterStorageKind;
        newValue.Value.r.Register = AllocateRegister();

        op.Code = IcOpSub;
        op.Mode = IcIntPtrType;
        op.rrr.R1 = ReadValue(baseValue);
        op.rrr.R2 = ReadValue(byteOffsetValue, &baseValue.Type);
        op.rrr.R3 = newValue.Value.r.Register;
        AppendOp(op);
    }
    else if (value1.Type.Kind == IcPointerTypeKind && value2.Type.Kind == IcPointerTypeKind && value1.Type.EqualTo(value2.Type))
    {
        int size = GetChildForPointerType(value1.Type).SizeOf();

        if (size == -1)
            throw "cannot calculate size of type";

        IcTypedValue byteDiffValue;

        memset(&byteDiffValue, 0, sizeof(IcTypedValue));
        byteDiffValue.Type = value1.Type;
        byteDiffValue.Value.Kind = IcRegisterStorageKind;
        byteDiffValue.Value.r.Register = AllocateRegister();

        op.Code = IcOpSub;
        op.Mode = IcIntPtrType;
        op.rrr.R1 = ReadValue(value1);
        op.rrr.R2 = ReadValue(value2);
        op.rrr.R3 = byteDiffValue.Value.r.Register;
        AppendOp(op);

        newValue.Type = _sizeT;
        newValue.Value.Kind = IcRegisterStorageKind;
        newValue.Value.r.Register = AllocateRegister();

        op.Code = IcOpDivUI;
        op.Mode = _sizeT.Base.Type;
        op.rcr.R1 = ReadValue(byteDiffValue, &_sizeT);
        op.rcr.C2 = CreateImmediateNumber(size, _sizeT.Base.Type);
        op.rcr.R3 = newValue.Value.r.Register;
        AppendOp(op);
    }
    else
    {
        throw "invalid type for '-' or '--' operator";
    }
}

void CParser::MultiplyExpression(std::string &tag, IcTypedValue &input1, IcTypedValue &input2, IcTypedValue &newValue)
{
    IcOperation op;
    IcTypedValue value1;
    IcTypedValue value2;
    IcType commonType;

    ConvertOneType(input1, 0, value1);
    ConvertOneType(input2, 0, value2);

    if (!CheckType(value1.Type, CAllowInt | CAllowFloat) || !CheckType(value2.Type, CAllowInt | CAllowFloat))
        throw "invalid type for '*' operator";

    CommonNumberType(value1.Type, value2.Type, commonType);

    memset(&newValue, 0, sizeof(IcTypedValue));
    newValue.Type = commonType;
    newValue.Value.Kind = IcRegisterStorageKind;
    newValue.Value.r.Register = AllocateRegister();

    if (tag == "Multiply")
        op.Code = (commonType.Traits & IcUnsignedTrait) ? IcOpMulU : IcOpMul;
    else if (tag == "Divide")
        op.Code = (commonType.Traits & IcUnsignedTrait) ? IcOpDivU : IcOpDiv;
    else if (tag == "Modulo")
        op.Code = (commonType.Traits & IcUnsignedTrait) ? IcOpModU : IcOpMod;

    op.Mode = commonType.Base.Type;
    op.rrr.R1 = ReadValue(value1, &commonType);
    op.rrr.R2 = ReadValue(value2, &commonType);
    op.rrr.R3 = newValue.Value.r.Register;
    AppendOp(op);
}

void CParser::CastExpression(IcType &type, IcTypedValue &input, IcTypedValue &newValue)
{
    IcTypedValue value;

    ConvertOneType(input, 0, value);

    if (CheckType(value.Type, CAllowInt | CAllowFloat | CAllowPointer) && CheckType(type, CAllowInt | CAllowFloat | CAllowPointer))
    {
        memset(&newValue, 0, sizeof(IcTypedValue));
        newValue.Type = type;
        newValue.Value.Kind = IcRegisterStorageKind;
        newValue.Value.r.Register = ReadValue(value, &type);
    }
    else if (CheckType(value.Type, CAllowStruct) && CheckType(type, CAllowStruct) && value.Type.EqualTo(type))
    {
        newValue = value;
    }
    else
    {
        throw "invalid type for cast operator";
    }
}

void CParser::CallExpression(IcTypedValue &input, CParameterList *parameters, IcTypedValue &newValue)
{
    IcTypedValue value;
    IcFunctionType *functionType;
    IcOperation op;
    IcRegister structReturnRegister = 0;

    SequencePoint();
    ConvertOneType(input, 0, value);

    if (input.Type.Kind != IcPointerTypeKind || input.Type.Pointer.Child->Kind != IcFunctionTypeKind)
        throw "invalid type for call operator";

    functionType = input.Type.Pointer.Child->Function;

    // Set up the call operation and return value.

    op.Code = CheckType(functionType->ReturnType, CAllowStruct | CAllowVoid) ? IcOpCall : IcOpCallV;
    op.rrsrx.R1 = ReadValue(input);
    op.rrsrx.Rs = new IcRegisterList;

    if (op.Code == IcOpCallV)
        op.rrsrx.RX = AllocateRegister();

    if (CheckType(functionType->ReturnType, CAllowInt | CAllowFloat | CAllowPointer))
    {
        op.Mode = GetBaseType(functionType->ReturnType);
    }
    else if (CheckType(functionType->ReturnType, CAllowStruct))
    {
        IcOperation subOp;
        int size;

        size = functionType->ReturnType.SizeOf();

        if (size == -1)
            throw "cannot calculate size of type";

        structReturnRegister = AllocateRegister();
        subOp.Code = IcOpLdF;
        subOp.Mode = IcIntPtrType;
        subOp.cr.C1 = CreateImmediateNumber(_functionScope->AllocateSlot(size, 0), IcIntType);
        subOp.cr.R2 = structReturnRegister;
        AppendOp(subOp);

        op.rrsrx.Rs->push_back(structReturnRegister);
    }

    // Add the parameters.

    size_t i = 0;

    if (parameters && parameters->size() != 0)
    {
        for (auto it = parameters->begin(); it != parameters->end(); ++it)
        {
            IcTypedValue argument;
            IcType actualType;

            ConvertOneType(*it, 0, argument);

            if (i < functionType->Parameters.size())
            {
                actualType = functionType->Parameters[i];

                // Perform type checking.

                if (CheckType(actualType, CAllowInt | CAllowFloat) && CheckType(argument.Type, CAllowInt | CAllowFloat))
                {
                    // Nothing
                }
                else if (CheckType(actualType, CAllowStruct) && CheckType(argument.Type, CAllowStruct))
                {
                    if (!actualType.EqualTo(argument.Type))
                        throw "type mismatch in argument to function";
                }
                else if (CheckType(actualType, CAllowPointer) && CheckType(argument.Type, CAllowPointer))
                {
                    if (!CheckCompatiblePointerTypes(actualType, argument.Type, true))
                        throw "pointer type mismatch in argument to function";
                }
                else if (CheckType(actualType, CAllowPointer) && CheckType(argument.Type, CAllowInt) && IsConstantZero(argument.Value))
                {
                    // Nothing
                }
                else
                {
                    throw "type mismatch in argument to function";
                }
            }
            else
            {
                if (!functionType->Variadic)
                    throw "too many arguments to function";
            }

            if (CheckType(argument.Type, CAllowInt | CAllowFloat | CAllowPointer))
            {
                if (i >= functionType->Parameters.size())
                {
                    actualType = argument.Type;

                    if (CheckType(actualType, CAllowFloat))
                        actualType.Base.Type = IcDoubleType;
                }

                op.rrsrx.Rs->push_back(ReadValue(argument, &actualType));
            }
            else if (CheckType(argument.Type, CAllowStruct))
            {
                int remainingBytes = argument.Type.SizeOf();
                int offset = 0;

                assert(argument.Value.Kind == IcIndirectStorageKind);

                if (remainingBytes == -1)
                    throw "cannot calculate size of type";

                while (remainingBytes != 0)
                {
                    IcOperation subOp;
                    IcRegister addressRegister;

                    addressRegister = argument.Value.i.Address;

                    if (offset != 0)
                    {
                        addressRegister = AllocateRegister();
                        subOp.Code = IcOpAddI;
                        subOp.Mode = IcIntPtrType;
                        subOp.rcr.R1 = argument.Value.i.Address;
                        subOp.rcr.C2 = CreateImmediateNumber(offset, IcIntPtrType);
                        subOp.rcr.R3 = addressRegister;
                        AppendOp(subOp);
                    }

                    subOp.Code = IcOpLd;
                    subOp.rr.R1 = addressRegister;
                    subOp.rr.R2 = AllocateRegister();

                    if (remainingBytes >= 4)
                    {
                        subOp.Mode = IcIntType;
                        offset += 4;
                        remainingBytes -= 4;
                    }
                    else if (remainingBytes >= 2)
                    {
                        subOp.Mode = IcShortType;
                        offset += 2;
                        remainingBytes -= 2;
                    }
                    else if (remainingBytes >= 1)
                    {
                        subOp.Mode = IcCharType;
                        offset += 1;
                        remainingBytes -= 1;
                    }

                    AppendOp(subOp);
                    op.rrsrx.Rs->push_back(subOp.rr.R2);
                }
            }

            i++;
        }
    }

    if (i < functionType->Parameters.size())
        throw "too few arguments to function";

    AppendOp(op);

    // Create the expression result from the return value.

    memset(&newValue, 0, sizeof(IcTypedValue));
    newValue.Type = functionType->ReturnType;

    if (CheckType(functionType->ReturnType, CAllowInt | CAllowFloat | CAllowPointer))
    {
        newValue.Value.Kind = IcRegisterStorageKind;
        newValue.Value.r.Register = op.rrsrx.RX;
    }
    else if (CheckType(functionType->ReturnType, CAllowStruct))
    {
        newValue.Value.Kind = IcIndirectStorageKind;
        newValue.Value.i.Address = structReturnRegister;
    }
    else
    {
        assert(CheckType(functionType->ReturnType, CAllowVoid));
    }
}

void CParser::DefinitionList(CDeclarationList *declarationList)
{
    for (auto it = declarationList->begin(); it != declarationList->end(); ++it)
    {
        std::string &name = it->Declarator.FindName();

        if (name.length() == 0)
        {
            // TODO: Warning if type is untagged structure
            continue;
        }

        IcType type = CreateIcType(it->Type, it->Qualifiers, it->Declarator, it->TypeSpecified);

        if ((unsigned int)it->Storage & CTypedefClass)
        {
            if (_scope->Symbols.find(name) != _scope->Symbols.end())
                throw "symbol already in use";

            IcOrdinarySymbol *ordinarySymbol = new IcOrdinarySymbol;

            ordinarySymbol->Kind = IcTypedefSymbolKind;
            ordinarySymbol->Name = name;
            ordinarySymbol->t.Target = type;
            _scope->Symbols[name] = ordinarySymbol;

            if (it->Initializer)
                throw "initializer used but no variable declared";
        }
        else
        {
            if (_functionScope && ((unsigned int)it->Storage & CExternClass))
                throw "'extern' illegal on variable with block scope";

            // TODO:
            // char a[] = "asdf";
            // or
            // int b[] = { 1, 2, 3, 4 };

            if (_functionScope && !((unsigned int)it->Storage & CStaticClass))
            {
                // Local variable

                IcTypedValue value;
                IcVariableId variableId;

                if (_scope->Symbols.find(name) != _scope->Symbols.end())
                    throw "symbol already in use";

                value.Type = type;
                value.Value = AllocateVariable(type, &variableId);

                IcOrdinarySymbol *ordinarySymbol = new IcOrdinarySymbol;

                ordinarySymbol->Kind = IcVariableSymbolKind;
                ordinarySymbol->Name = name;
                ordinarySymbol->v.Id = variableId;
                _scope->Symbols[name] = ordinarySymbol;

                if (it->Initializer)
                {
                    if (CheckType(type, CAllowInt | CAllowFloat | CAllowPointer))
                    {
                        IcTypedValue *scalarValue;
                        IcTypedValue dummy;

                        if (!it->Initializer->IsList)
                        {
                            scalarValue = it->Initializer->Value;
                        }
                        else
                        {
                            if (it->Initializer->List->size() != 1 || it->Initializer->List->begin()->IsList)
                                throw "too many initializers";

                            scalarValue = it->Initializer->List->begin()->Value;
                        }

                        AssignExpressionAssign(value, *scalarValue, dummy);
                    }
                    else
                    {
                        // TODO
                    }
                }
            }
            else
            {
                // Global or static variable/function

                if (_functionScope && type.Kind == IcFunctionTypeKind)
                    throw "nested functions are illegal";

                auto symbolIt = _scope->Symbols.find(name);
                bool define = true;
                bool import = false;

                if (((unsigned int)it->Storage & CExternClass) || type.Kind == IcFunctionTypeKind)
                {
                    if (symbolIt != _scope->Symbols.end())
                    {
                        if (symbolIt->second->Kind != IcObjectSymbolKind)
                            throw "conflicting redefinition of symbol";
                        if (!symbolIt->second->o.Type.EqualTo(type))
                            throw "conflicting redefinition of symbol";

                        define = false;
                    }

                    import = true;
                }
                else
                {
                    if (symbolIt != _scope->Symbols.end())
                        throw "symbol already in use";
                }

                if (define)
                {
                    IcObject *object;

                    if (import)
                    {
                        object = _objectManager->CreateImport(name);
                    }
                    else
                    {
                        int size = type.SizeOf();

                        if (size == -1)
                            throw "cannot calculate size of type";

                        if (it->Initializer)
                        {
                            // TODO
                        }

                        object = _objectManager->CreateData(((unsigned int)it->Storage & CStaticClass) ? NULL : &name, NULL, size);
                    }

                    IcOrdinarySymbol *ordinarySymbol = new IcOrdinarySymbol;

                    ordinarySymbol->Kind = IcObjectSymbolKind;
                    ordinarySymbol->Name = name;
                    ordinarySymbol->o.Type = type;
                    ordinarySymbol->o.Reference = object->Id;
                    _scope->Symbols[name] = ordinarySymbol;
                }
            }
        }
    }
}

void CParser::CreateFunctionScope(CDeclaration *declaration, CDeclarator *declarator)
{
    std::string &name = declarator->FindName();

    if (name.length() == 0)
        throw "function must have a name";

    _functionScope = new IcFunctionScope;

    IcType functionType = CreateIcType(declaration->Type, declaration->Qualifiers, *declarator, declaration->TypeSpecified, true);

    auto symbolIt = _scope->Symbols.find(name);

    if (symbolIt != _scope->Symbols.end())
    {
        if (symbolIt->second->Kind != IcObjectSymbolKind)
            throw "conflicting redefinition of symbol";
        if (!symbolIt->second->o.Type.EqualTo(functionType))
            throw "conflicting redefinition of symbol";
    }

    IcOrdinarySymbol *ordinarySymbol = new IcOrdinarySymbol;

    ordinarySymbol->Kind = IcObjectSymbolKind;
    ordinarySymbol->Name = name;
    ordinarySymbol->o.Type = functionType;
    ordinarySymbol->o.Reference = _objectManager->CreateImport(name)->Id;
    _scope->Symbols[name] = ordinarySymbol;

    _functionScope->StartBlock = _functionScope->CreateBlock();
    _functionScope->EndBlock = _functionScope->CreateBlock();
    _functionScope->Type = functionType;
    _functionScope->Symbol = ordinarySymbol;
    _currentBlock = _functionScope->StartBlock;

    if (CheckType(functionType.Function->ReturnType, CAllowInt | CAllowFloat | CAllowPointer | CAllowStruct))
    {
        AllocateVariable(functionType.Function->ReturnType, &_functionScope->ReturnVariableId);
    }
    else if (!CheckType(functionType.Function->ReturnType, CAllowVoid))
    {
        throw "invalid return type for function";
    }

    IcRegister baseRegister = 0;
    int i = 0;
    int offset = 0;

    if (CheckType(functionType.Function->ReturnType, CAllowStruct))
        offset = _sizeT.SizeOf(); // leave room for pointer to struct return value

    for (auto it = functionType.Function->Parameters.begin(); it != functionType.Function->Parameters.end(); ++it)
    {
        IcType &parameterType = *it;
        IcVariableId variableId;
        IcFunctionVariable functionVariable;
        int size;
        IcOrdinarySymbol *ordinarySymbol;
        IcOperation op;

        size = parameterType.SizeOf();

        if (size == -1)
            throw "cannot calculate size of type";

        std::string &parameterName = (*functionType.Function->ParameterNames)[i];

        if (parameterName.length() == 0)
            throw "parameter must have a name";

        variableId = AllocateVariableId();
        functionVariable.Parameter = true;
        functionVariable.Index = offset;
        functionVariable.AddressRegister = AllocateRegister();
        functionVariable.Type = parameterType;
        _functionScope->Variables[variableId] = functionVariable;

        ordinarySymbol = new IcOrdinarySymbol;
        ordinarySymbol->Kind = IcVariableSymbolKind;
        ordinarySymbol->Name = parameterName;
        ordinarySymbol->v.Id = variableId;
        _functionScope->PendingSymbols.push_back(ordinarySymbol);

        if (baseRegister == 0)
        {
            baseRegister = AllocateRegister();
            op.Code = IcOpLdFA;
            op.Mode = IcIntPtrType;
            op.r.R1 = baseRegister;
            AppendOp(op);
        }

        op.Code = IcOpAddI;
        op.Mode = IcIntPtrType;
        op.rcr.R1 = baseRegister;
        op.rcr.C2 = CreateImmediateNumber(offset, IcIntPtrType);
        op.rcr.R3 = functionVariable.AddressRegister;
        AppendOp(op);

        i++;
        offset = (offset + size + _sizeT.SizeOf() - 1) & ~(_sizeT.SizeOf() - 1);
    }
}

void CParser::DeleteFunctionScope()
{
    IcOperation op;

    op.Code = IcOpJmp;
    op.b.B1 = _functionScope->EndBlock;
    AppendOp(op);

    if (_functionScope->PendingGotos.size() != 0)
        throw "unresolved 'goto' statement(s)";

    _currentBlock = _functionScope->EndBlock;

    if (CheckType(_functionScope->Type.Function->ReturnType, CAllowInt | CAllowFloat | CAllowPointer))
    {
        IcRegister valueRegister = AllocateRegister();

        op.Code = IcOpLd;
        op.Mode = GetBaseType(_functionScope->Type.Function->ReturnType);
        op.rr.R1 = _functionScope->Variables[_functionScope->ReturnVariableId].AddressRegister;
        op.rr.R2 = valueRegister;
        AppendOp(op);

        op.Code = IcOpSrt;
        op.Mode = GetBaseType(_functionScope->Type.Function->ReturnType);
        op.r.R1 = valueRegister;
        AppendOp(op);
    }
    else if (CheckType(_functionScope->Type.Function->ReturnType, CAllowStruct))
    {
        int size = _functionScope->Type.Function->ReturnType.SizeOf();

        if (size == -1)
            throw "cannot calculate size of type";

        IcRegister argumentsRegister = AllocateRegister();
        IcRegister structReturnRegister = AllocateRegister();

        op.Code = IcOpLdFA;
        op.Mode = IcIntPtrType;
        op.r.R1 = argumentsRegister;
        AppendOp(op);

        op.Code = IcOpLd;
        op.Mode = IcIntPtrType;
        op.rr.R1 = argumentsRegister;
        op.rr.R2 = structReturnRegister;
        AppendOp(op);

        op = IcOperation();
        op.Code = IcOpCpyI;
        op.rrc.R1 = _functionScope->Variables[_functionScope->ReturnVariableId].AddressRegister;
        op.rrc.R2 = structReturnRegister;
        op.rrc.C3 = CreateImmediateNumber(size, IcIntType);
        AppendOp(op);
    }

    op = IcOperation();
    op.Code = IcOpEnd;
    AppendOp(op);

    _objectManager->CreateFunction(_functionScope);

    _currentBlock = NULL;
    delete _functionScope;
    _functionScope = NULL;
}
