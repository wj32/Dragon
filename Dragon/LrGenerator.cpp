#include "stdafx.h"
#include "LrGenerator.h"
#include "Lexer.h"

LrGenerator::~LrGenerator()
{
    for (auto it = _symbols.begin(); it != _symbols.end(); ++it)
        delete *it;
}

bool LrGenerator::Load(std::istream &stream)
{
    Lexer lexer(stream);
    Token token;

    lexer.Initialize();
    lexer.SetOptions(LEXER_ALLOW_LINE_COMMENTS | LEXER_ALLOW_DOLLAR_SIGN_IN_ID);
    _loadLexer = &lexer;
    _loadRetreatToken = false;

    while (true)
    {
        LrSymbol *lhsSymbol;

        // lhs = rhs; or EOF

        if (!GetLoadToken(token))
            break;

        if (token.Type != IdType)
        {
            SetError("identifier expected");
            return false;
        }

        lhsSymbol = AddSymbol(token.Text);

        if (token.Text == "__START__")
            _startSymbol = lhsSymbol;

        if (!GetLoadToken(token) || token.Type != OpEqualsType)
        {
            SetError("'=' expected");
            return false;
        }

        while (true)
        {
            std::vector<LrSymbol *> symbolList;
            std::string tag;
            LrAboveSet above;
            LrGroupSet group;
            LrProduction production;

            // symbol-list | symbol-list | ...

            while (true)
            {
                // symbol ... {tag} ... <attribute> ... symbol ...

                if (!GetLoadToken(token))
                    break;

                if (token.Type == SymLeftCurlyType)
                {
                    if (!GetLoadToken(token) || token.Type != IdType)
                    {
                        SetError("identifier expected");
                        return false;
                    }

                    tag = token.Text;

                    if (!GetLoadToken(token) || token.Type != SymRightCurlyType)
                    {
                        SetError("'}' expected");
                        return false;
                    }
                }
                else if (token.Type == OpLessType)
                {
                    if (!GetLoadToken(token) || token.Type != IdType)
                    {
                        SetError("attribute name expected");
                        return false;
                    }

                    if (token.Text == "Above")
                    {
                        std::string aboveSymbol;
                        std::string aboveTag;

                        // <Above(symbol)> or <Above(symbol.tag)>

                        if (!GetLoadToken(token) || token.Type != SymLeftRoundType)
                        {
                            SetError("'(' expected");
                            return false;
                        }

                        if (!GetLoadToken(token) || token.Type != IdType)
                        {
                            SetError("identifier expected");
                            return false;
                        }

                        aboveSymbol = token.Text;

                        if (GetLoadToken(token) && token.Type == OpDotType)
                        {
                            if (!GetLoadToken(token) || token.Type != IdType)
                            {
                                SetError("identifier expected");
                                return false;
                            }

                            aboveTag = token.Text;
                        }
                        else
                        {
                            RetreatLoadToken();
                        }

                        if (!GetLoadToken(token) || token.Type != SymRightRoundType)
                        {
                            SetError("')' expected");
                            return false;
                        }

                        above.insert(std::pair<std::string, std::string>(aboveSymbol, aboveTag));
                    }
                    else if (token.Text == "Group")
                    {
                        std::string groupName;

                        // <Group(name)>

                        if (!GetLoadToken(token) || token.Type != SymLeftRoundType)
                        {
                            SetError("'(' expected");
                            return false;
                        }

                        if (!GetLoadToken(token) || token.Type != IdType)
                        {
                            SetError("identifier expected");
                            return false;
                        }

                        groupName = token.Text;

                        if (!GetLoadToken(token) || token.Type != SymRightRoundType)
                        {
                            SetError("')' expected");
                            return false;
                        }

                        group.insert(groupName);
                    }
                    else
                    {
                        SetError("unrecognized attribute name");
                        return false;
                    }

                    if (!GetLoadToken(token) || token.Type != OpGreaterType)
                    {
                        SetError("'>' expected");
                        return false;
                    }
                }
                else
                {
                    if (token.Type != IdType && token.Type != LiteralStringType)
                    {
                        RetreatLoadToken();
                        break;
                    }

                    if (token.Type == LiteralStringType && token.Text.length() != 0 && token.Text[0] == 'L')
                    {
                        SetError("'L' prefix not allowed on strings");
                        return false;
                    }

                    symbolList.push_back(AddSymbol(token.Text));
                }
            }

            production.Id = _nextProductionId++;
            production.Tag = tag;
            production.Above = above;
            production.Group = group;
            production.Lhs = lhsSymbol;
            production.Rhs = symbolList;
            _productions.push_back(new LrProduction(production));
            _idProductionMap.insert(std::unordered_map<int, LrProduction *>::value_type(production.Id, _productions[_productions.size() - 1]));
            _lhsProductionMap.insert(std::unordered_multimap<LrSymbol *, LrProduction *>::value_type(lhsSymbol, _productions[_productions.size() - 1]));

            if (!GetLoadToken(token))
                break;
            if (token.Type != OpOrType)
            {
                RetreatLoadToken();
                break;
            }

            if (lhsSymbol == _startSymbol)
            {
                SetError("multiple productions for __START__");
                return false;
            }
        }

        if (!GetLoadToken(token) || token.Type != SymSemicolonType)
        {
            SetError("';' expected");
            return false;
        }
    }

    return true;
}

LrTable *LrGenerator::Generate()
{
    std::map<LrItemSet, int> collection;
    std::stack<std::shared_ptr<LrItemSet>> unseen;
    int nextStateId = 0;
    LrItem startItem;
    LrItemSet startSet;
    LrTable *table;
    std::map<std::pair<int, LrItem>, std::shared_ptr<LrItemSet>> queuedActions;

    PopulateTerminalSymbols();

    if (!GetStartItem(startItem))
    {
        SetError("__START__ production not defined");
        return NULL;
    }

    table = new LrTable;

    startSet.insert(startItem);
    unseen.push(std::shared_ptr<LrItemSet>(new LrItemSet(Closure(startSet))));

    while (unseen.size() != 0)
    {
        std::shared_ptr<LrItemSet> set = unseen.top();
        int stateId;
        std::unordered_map<LrSymbol *, std::shared_ptr<LrItemSet>> symbolSets;

        stateId = nextStateId;
        unseen.pop();
        collection.insert(std::map<LrItemSet, int>::value_type(*set, stateId));
        nextStateId++;

        // Queue shift/goto actions.

        for (auto it = set->begin(); it != set->end(); ++it)
        {
            if (it->Index < it->Production->Rhs.size())
            {
                LrSymbol *symbol = it->Production->Rhs[it->Index];
                LrItem newItem;
                LrItemSet newSet;

                newItem = *it;
                newItem.Index++;
                newSet.insert(newItem);
                newSet = Closure(newSet);

                if (symbolSets[symbol] == NULL)
                    symbolSets[symbol] = std::shared_ptr<LrItemSet>(new LrItemSet);

                symbolSets[symbol]->insert(newSet.begin(), newSet.end());
            }
        }

        for (auto it = set->begin(); it != set->end(); ++it)
        {
            if (it->Index < it->Production->Rhs.size())
            {
                LrSymbol *symbol = it->Production->Rhs[it->Index];

                queuedActions[std::pair<int, LrItem>(stateId, *it)] = symbolSets[symbol];
            }
        }

        for (auto it = symbolSets.begin(); it != symbolSets.end(); ++it)
        {
            if (collection.find(*(it->second)) == collection.end())
                unseen.push(it->second);
        }

        // Create reduce/accept actions.

        for (auto it = set->begin(); it != set->end(); ++it)
        {
            LrAction action;
            std::pair<int, int> tableKey;

            if (it->Index == it->Production->Rhs.size())
            {
                if (it->Production->Lhs == _startSymbol)
                {
                    action.Type = LrAccept;
                    action.Id = 0;
                }
                else
                {
                    action.Type = LrReduce;
                    action.Id = it->Production->Id;
                }

                tableKey = std::pair<int, int>(stateId, it->Lookahead->Id);
                auto range = table->equal_range(tableKey);

                if (range.first != range.second && !CanOverwriteAction(range.first->second, action, it->Production))
                {
                    if (CanJoinGroup(table, tableKey, it->Production))
                    {
                        table->insert(LrTable::value_type(tableKey, action));
                    }
                    else
                    {
                        SetAmbiguousGrammarError(table, tableKey, action.Type, it->Production, it->Index, it->Lookahead);
                        delete table;
                        return NULL;
                    }
                }
                else
                {
                    table->erase(range.first, range.second);
                    table->insert(LrTable::value_type(tableKey, action));
                }
            }
        }
    }

    // Create shift/goto actions.

    for (auto it = queuedActions.begin(); it != queuedActions.end(); ++it)
    {
        LrAction action;
        std::pair<int, int> tableKey;

        if (it->first.second.Index < it->first.second.Production->Rhs.size())
        {
            if (IsTerminalSymbol(it->first.second.Production->Rhs[it->first.second.Index]))
            {
                action.Type = LrShift;
            }
            else
            {
                action.Type = LrGoto;
            }

            action.Id = collection[*(it->second)];

            tableKey = std::pair<int, int>(it->first.first, it->first.second.Production->Rhs[it->first.second.Index]->Id);
            auto range = table->equal_range(tableKey);

            if (range.first != range.second && !CanOverwriteAction(range.first->second, action, it->first.second.Production))
            {
                SetAmbiguousGrammarError(table, tableKey, action.Type, it->first.second.Production, it->first.second.Index, it->first.second.Lookahead);
                delete table;
                return NULL;
            }

            table->erase(range.first, range.second);
            table->insert(LrTable::value_type(tableKey, action));
        }
    }

    return table;
}

bool LrGenerator::GetError(std::string &errorMessage)
{
    if (_error)
    {
        errorMessage = _errorMessage;
        return true;
    }
    else
    {
        return false;
    }
}

LrSymbol *LrGenerator::GetSymbol(std::string &text)
{
    auto it = _symbolMap.find(text);

    if (it != _symbolMap.end())
        return it->second;

    return NULL;
}

LrSymbol *LrGenerator::GetSymbol(int id)
{
    auto it = _idSymbolMap.find(id);

    if (it != _idSymbolMap.end())
        return it->second;

    return NULL;
}

LrSymbol *LrGenerator::GetEofSymbol()
{
    return _eofSymbol;
}

LrProduction *LrGenerator::GetProduction(int id)
{
    auto it = _idProductionMap.find(id);

    if (it != _idProductionMap.end())
        return it->second;

    return NULL;
}

bool LrGenerator::IsTerminalSymbol(LrSymbol *symbol)
{
    return _terminalSymbols.find(symbol) != _terminalSymbols.end();
}

void LrGenerator::SetError(std::string &errorMessage)
{
    _error = true;
    _errorMessage = errorMessage;
}

void LrGenerator::SetError(const char *errorMessage)
{
    _error = true;
    _errorMessage = errorMessage;
}

bool LrGenerator::GetLoadToken(Token &token)
{
    bool result;

    if (_loadRetreatToken)
    {
        token = _loadToken;
        _loadRetreatToken = false;
        return true;
    }
    else
    {
        while (true)
        {
            result = _loadLexer->GetToken(_loadToken);

            if (!result)
                return false;

            if (_loadToken.Type != CommentType && _loadToken.Type != LineCommentType)
                break;
        }

        token = _loadToken;
        return result;
    }
}

void LrGenerator::RetreatLoadToken()
{
    _loadRetreatToken = true;
}

LrSymbol *LrGenerator::AddSymbol(std::string &text)
{
    auto it = _symbolMap.find(text);
    LrSymbol symbol;

    if (it != _symbolMap.end())
    {
        return it->second;
    }

    symbol.Id = _nextSymbolId++;
    symbol.Text = text;

    _symbols.push_back(new LrSymbol(symbol));
    auto pair = _symbolMap.insert(std::unordered_map<std::string, LrSymbol *>::value_type(text, _symbols[_symbols.size() - 1]));
    _idSymbolMap[symbol.Id] = _symbols[_symbols.size() - 1];

    return pair.first->second;
}

void LrGenerator::PopulateTerminalSymbols()
{
    _terminalSymbols.clear();

    for (auto it = _symbols.begin(); it != _symbols.end(); ++it)
    {
        if (_lhsProductionMap.find(*it) == _lhsProductionMap.end())
            _terminalSymbols.insert(*it);
    }
}

bool LrGenerator::GetStartItem(LrItem &item)
{
    auto it = _lhsProductionMap.find(_startSymbol);

    if (it == _lhsProductionMap.end())
        return false;

    item.Production = it->second;
    item.Index = 0;
    item.Lookahead = _eofSymbol;

    return true;
}

LrItemSet LrGenerator::Closure(LrItemSet set)
{
    LrItemSet result;
    std::stack<LrItem> unseen;

    for (auto it = set.begin(); it != set.end(); ++it)
    {
        unseen.push(*it);
    }

    while (!unseen.empty())
    {
        auto item = unseen.top();

        unseen.pop();
        result.insert(item);

        if (item.Index < item.Production->Rhs.size())
        {
            LrSymbol *symbol = item.Production->Rhs[item.Index];

            if (!IsTerminalSymbol(symbol))
            {
                LrItem nextItem;
                LrSymbolSet lookaheadSet;

                nextItem = item;
                nextItem.Index++;
                lookaheadSet = First(nextItem);

                auto itPair = _lhsProductionMap.equal_range(symbol);

                for (auto it = itPair.first; it != itPair.second; ++it)
                {
                    LrItem newItem;

                    newItem.Production = it->second;
                    newItem.Index = 0;

                    for (auto it2 = lookaheadSet.begin(); it2 != lookaheadSet.end(); ++it2)
                    {
                        newItem.Lookahead = *it2;

                        if (result.find(newItem) == result.end())
                            unseen.push(newItem);
                    }
                }
            }
        }
    }

    return result;
}

LrSymbolSet LrGenerator::First(LrItem item)
{
    auto cacheIt = _firstMap.find(item);

    if (cacheIt != _firstMap.end())
        return cacheIt->second;

    if (item.Index == item.Production->Rhs.size())
    {
        LrSymbolSet set;
        set.insert(item.Lookahead);
        return set;
    }
    else
    {
        if (IsTerminalSymbol(item.Production->Rhs[item.Index]))
        {
            LrSymbolSet set;
            set.insert(item.Production->Rhs[item.Index]);
            return set;
        }
        else
        {
            LrSymbolSet result;
            LrSymbolSet firstAfter;
            LrItem newItem;

            // Insert the item into the cache before we compute the symbol set.
            // This will take care of recursion within productions.
            _firstMap[item] = LrSymbolSet();

            newItem = item;
            newItem.Index++;
            firstAfter = First(newItem);

            if (firstAfter.size() == 0)
                firstAfter.insert(item.Lookahead);

            auto itPair = _lhsProductionMap.equal_range(item.Production->Rhs[item.Index]);

            for (auto it = itPair.first; it != itPair.second; ++it)
            {
                newItem.Production = it->second;
                newItem.Index = 0;

                for (auto it2 = firstAfter.begin(); it2 != firstAfter.end(); ++it2)
                {
                    LrSymbolSet first;

                    newItem.Lookahead = *it2;
                    first = First(newItem);
                    result.insert(first.begin(), first.end());
                }
            }

            _firstMap[item] = result;

            return result;
        }
    }
}

bool LrGenerator::CanOverwriteAction(LrAction existingAction, LrAction newAction, LrProduction *newProduction)
{
    LrProduction *existingProduction;

    if (existingAction.Type == newAction.Type && existingAction.Id == newAction.Id)
        return true;

    if (existingAction.Type != LrReduce)
        return false;

    existingProduction = GetProduction(existingAction.Id);

    if (IsProductionAbove(newProduction, existingProduction))
        return true;

    return false;
}

bool LrGenerator::IsProductionAbove(LrProduction *production1, LrProduction *production2)
{
    return production1->Above.find(std::pair<std::string, std::string>(production2->Lhs->Text, production2->Tag)) != production1->Above.end();
}

bool LrGenerator::CanJoinGroup(LrTable *table, std::pair<int, int> tableKey, LrProduction *newProduction)
{
    auto range = table->equal_range(tableKey);

    if (range.first->second.Type != LrReduce)
        return false;

    if (newProduction->Group.size() == 0)
        return false;

    LrGroupSet set = newProduction->Group;

    for (auto it = range.first; it != range.second; ++it)
    {
        // Intersect this set with the current set.

        if (it->second.Type != LrReduce)
            return false;

        LrProduction *production = GetProduction(it->second.Id);
        LrGroupSet newSet;

        for (auto it2 = set.begin(); it2 != set.end(); ++it2)
        {
            if (production->Group.find(*it2) != production->Group.end())
                newSet.insert(*it2);
        }

        if (newSet.size() == 0)
            return false;

        set = newSet;
    }

    return true;
}

void LrGenerator::SetAmbiguousGrammarError(LrTable *table, std::pair<int, int> tableKey, LrActionType actionType, LrProduction *production, size_t index, LrSymbol *lookahead)
{
    std::string message = "Ambiguous or non-LR(1) grammar: ";
    char buffer[11];
    LrAction existingAction;
    auto existingActionIt = table->find(tableKey);

    existingAction = existingActionIt->second;

    message += GetActionTypeString(actionType);
    message += " on (";
    message += production->Lhs->Text;

    if (production->Tag.length() != 0)
        message += " {" + production->Tag + "}";

    message += " = ";
    GetProductionRhsString(message, production);

    sprintf(buffer, "%u", (unsigned int)index);
    message += ", ";
    message += buffer;
    message += ", '";
    message += lookahead->Text;
    message += "') replacing ";
    message += GetActionTypeString(existingAction.Type);

    switch (existingAction.Type)
    {
    case LrReduce:
        {
            LrProduction *existingProduction = GetProduction(existingAction.Id);
            message += " on (";
            message += existingProduction->Lhs->Text;

            if (existingProduction->Tag.length() != 0)
                message += " {" + existingProduction->Tag + "}";

            message += " = ";
            GetProductionRhsString(message, existingProduction);

            message += ")";
        }
        break;
    }

    SetError(message);
}

const char *LrGenerator::GetActionTypeString(LrActionType actionType)
{
    switch (actionType)
    {
    case LrShift:
        return "SHIFT";
    case LrGoto:
        return "GOTO";
    case LrReduce:
        return "REDUCE";
    case LrAccept:
        return "ACCEPT";
    default:
        return "???";
    }
}

void LrGenerator::GetProductionRhsString(std::string &appendTo, LrProduction *production)
{
    for (size_t i = 0; i < production->Rhs.size(); i++)
    {
        appendTo += production->Rhs[i]->Text;

        if (i != production->Rhs.size() - 1)
            appendTo += " ";
    }
}
