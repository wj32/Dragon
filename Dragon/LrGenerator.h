#pragma once

#include "LrParser.h"
#include "Lexer.h"
#include "Token.h"

struct LrSymbol
{
    int Id;
    std::string Text;

    bool operator ==(const LrSymbol &other) const
    {
        return Id == other.Id;
    }

    bool operator !=(const LrSymbol &other) const
    {
        return !operator==(other);
    }

    bool operator <(const LrSymbol &other) const
    {
        return Id < other.Id;
    }
};

typedef std::set<std::pair<std::string, std::string>> LrAboveSet;
typedef std::set<std::string> LrGroupSet;

struct LrProduction
{
    int Id;
    std::string Tag;
    LrAboveSet Above;
    LrGroupSet Group;

    LrSymbol *Lhs;
    std::vector<LrSymbol *> Rhs;

    bool operator ==(const LrProduction &other) const
    {
        return Id == other.Id;
    }

    bool operator !=(const LrProduction &other) const
    {
        return !operator==(other);
    }
};

struct LrItem
{
    LrProduction *Production;
    size_t Index;
    LrSymbol *Lookahead;

    bool operator ==(const LrItem &other) const
    {
        if (*Production != *other.Production)
            return false;
        if (Index != other.Index)
            return false;
        if (*Lookahead != *other.Lookahead)
            return false;

        return true;
    }

    bool operator !=(const LrItem &other) const
    {
        return !operator==(other);
    }

    bool operator <(const LrItem &other) const
    {
        if (Production->Id != other.Production->Id)
            return Production->Id < other.Production->Id;
        if (Index != other.Index)
            return Index < other.Index;

        return Lookahead->Id < other.Lookahead->Id;
    }
};

struct LrItemFunctions
{
    size_t operator ()(const LrItem &value) const
    {
        return value.Production->Id + value.Index * 4;
    }
};

typedef std::set<LrSymbol *> LrSymbolSet;
typedef std::set<LrItem> LrItemSet;

class LrGenerator
{
public:
    LrGenerator()
        : _nextSymbolId(0), _nextProductionId(1), _startSymbol(NULL), _loadRetreatToken(false), _error(false)
    {
        std::string empty;
        _eofSymbol = AddSymbol(empty);
    }

    ~LrGenerator();

    bool Load(std::istream &stream);
    LrTable *Generate();
    bool GetError(std::string &errorMessage);
    LrSymbol *GetSymbol(std::string &text);
    LrSymbol *GetSymbol(int id);
    LrSymbol *GetEofSymbol();
    LrProduction *GetProduction(int id);
    bool IsTerminalSymbol(LrSymbol *symbol);

private:
    std::vector<LrSymbol *> _symbols;
    std::unordered_map<std::string, LrSymbol *> _symbolMap;
    std::unordered_map<int, LrSymbol *> _idSymbolMap;
    int _nextSymbolId;
    int _nextProductionId;
    std::vector<LrProduction *> _productions;
    std::unordered_map<int, LrProduction *> _idProductionMap;
    std::unordered_multimap<LrSymbol *, LrProduction *> _lhsProductionMap;
    LrSymbol *_eofSymbol;
    LrSymbol *_startSymbol;
    std::unordered_set<LrSymbol *> _terminalSymbols;
    std::unordered_map<LrItem, LrSymbolSet, LrItemFunctions> _firstMap;

    Lexer *_loadLexer;
    Token _loadToken;
    bool _loadRetreatToken;

    bool _error;
    std::string _errorMessage;

    void SetError(std::string &errorMessage);
    void SetError(const char *errorMessage);

    bool GetLoadToken(Token &token);
    void RetreatLoadToken();

    LrSymbol *AddSymbol(std::string &text);
    void PopulateTerminalSymbols();
    bool GetStartItem(LrItem &item);
    LrItemSet Closure(LrItemSet set);
    LrSymbolSet First(LrItem item);
    bool CanOverwriteAction(LrAction existingAction, LrAction newAction, LrProduction *newProduction);
    bool IsProductionAbove(LrProduction *production1, LrProduction *production2);
    bool CanJoinGroup(LrTable *table, std::pair<int, int> tableKey, LrProduction *newProduction);
    void SetAmbiguousGrammarError(LrTable *table, std::pair<int, int> tableKey, LrActionType actionType, LrProduction *production, size_t index, LrSymbol *lookahead);
    const char *GetActionTypeString(LrActionType actionType);
    void GetProductionRhsString(std::string &appendTo, LrProduction *production);
};
