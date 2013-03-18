#pragma once

enum LrActionType
{
    LrShift,
    LrGoto,
    LrReduce,
    LrAccept,
    LrError = -1
};

struct LrAction
{
    LrActionType Type;
    int Id;
};

typedef std::multimap<std::pair<int, int>, LrAction> LrTable;

struct LrTableStreamHeader
{
    unsigned int NumberOfEntries;
    unsigned int Reserved[3];
};

struct LrTableStreamEntry
{
    union
    {
        struct
        {
            int State : 30;
            int SetAction : 1; // RLE for action (struct s below)
            int SetState : 1; // RLE for state (u.State)
        };
        struct
        {
            int Lookahead : 30;
            int ZeroInit : 2;
        };
    } u;
    struct
    {
        int Id : 29;
        int Type : 3;
    } s;
};

struct LrParserItem
{
    int State;
    int Symbol;
    void *Context;
};

class LrParser
{
public:
    static void SaveTable(std::ostream &stream, LrTable *table);
    static LrTable *LoadTable(std::istream &stream);
    static LrActionType Step(LrParser *parser, int symbol, void *context);
    static LrActionType Consume(LrParser *parser, int symbol, void *context);

    virtual LrTable *Table() = 0;
    virtual LrParserItem Top() = 0;
    virtual void Push(LrParserItem item) = 0;
    virtual void Pop(int count) = 0;
    virtual int ProductionLength(int productionId) = 0;
    virtual int ProductionSymbol(int productionId) = 0;
    virtual LrAction ProductionConflict(LrTable::const_iterator begin, LrTable::const_iterator end) { LrAction action = { LrError, -1 }; return action; }

    virtual void Shift(int symbol) { }
    virtual void *BeforeReduce(int productionId) { return NULL; }
    virtual void AfterReduce(int productionId, void *context) { }
    virtual void Accept() { }
};
