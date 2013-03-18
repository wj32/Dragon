#include "stdafx.h"
#include "LrParser.h"

void LrParser::SaveTable(std::ostream &stream, LrTable *table)
{
    LrTableStreamHeader header = { table->size() };
    LrTableStreamEntry entry;
    int currentState = -1;
    LrAction currentAction = { LrError, -1 };

    stream.write((char *)&header, sizeof(header));

    for (auto it = table->begin(); it != table->end(); ++it)
    {
        if (currentState != it->first.first)
        {
            currentState = it->first.first;
            entry.u.State = currentState;
            entry.u.ZeroInit = 0;
            entry.u.SetState = 1;
            stream.write((char *)&entry.u, sizeof(entry.u));
        }

        entry.u.Lookahead = it->first.second;
        entry.u.ZeroInit = 0;

        if (currentAction.Type != it->second.Type || currentAction.Id != it->second.Id)
        {
            currentAction = it->second;
            entry.u.SetAction = 1;
            entry.s.Id = it->second.Id;
            entry.s.Type = it->second.Type;
            stream.write((char *)&entry, sizeof(entry));
        }
        else
        {
            stream.write((char *)&entry, sizeof(entry.u));
        }
    }
}

LrTable *LrParser::LoadTable(std::istream &stream)
{
    LrTable *table;
    LrTableStreamHeader header;
    LrTableStreamEntry entry;
    unsigned int numberOfEntries;
    int currentState = -1;
    LrAction currentAction = { LrError, -1 };
    int lookahead;

    stream.read((char *)&header, sizeof(header));

    if (stream.eof())
        return NULL;

    table = new LrTable;
    numberOfEntries = header.NumberOfEntries;

    while (numberOfEntries != 0)
    {
        stream.read((char *)&entry.u, sizeof(entry.u));

        if (stream.eof())
        {
            delete table;
            return NULL;
        }

        if (entry.u.SetState)
        {
            currentState = entry.u.State;
            continue;
        }

        if (entry.u.SetAction)
        {
            stream.read((char *)&entry.s, sizeof(entry.s));

            if (stream.eof())
            {
                delete table;
                return NULL;
            }

            currentAction.Id = entry.s.Id;
            currentAction.Type = (LrActionType)entry.s.Type;
        }

        lookahead = entry.u.Lookahead;

        table->insert(LrTable::value_type(std::pair<int, int>(currentState, lookahead), currentAction));

        numberOfEntries--;
    }

    return table;
}

LrActionType LrParser::Step(LrParser *parser, int symbol, void *context)
{
    auto actionRange = parser->Table()->equal_range(std::pair<int, int>(parser->Top().State, symbol));
    LrAction action;

    if (actionRange.first == actionRange.second)
        return LrError;

    auto actionFirstNext = actionRange.first;
    ++actionFirstNext;

    if (actionFirstNext != actionRange.second)
    {
        action = parser->ProductionConflict(actionRange.first, actionRange.second);
    }
    else
    {
        action = actionRange.first->second;
    }

    switch (action.Type)
    {
    case LrShift:
        {
            LrParserItem item;

            item.State = action.Id;
            item.Symbol = symbol;
            item.Context = context;
            parser->Push(item);

            parser->Shift(symbol);
        }
        break;
    case LrReduce:
        {
            int length;
            int symbolId;
            void *context;
            LrParserItem item;

            length = parser->ProductionLength(action.Id);
            symbolId = parser->ProductionSymbol(action.Id);

            context = parser->BeforeReduce(action.Id);
            parser->Pop(length);

            auto gotoIt = parser->Table()->find(std::pair<int, int>(parser->Top().State, symbolId));

            if (gotoIt == parser->Table()->end() || gotoIt->second.Type != LrGoto)
                return LrError;

            item.State = gotoIt->second.Id;
            item.Symbol = symbolId;
            item.Context = context;
            parser->Push(item);

            parser->AfterReduce(action.Id, context);
        }
        break;
    case LrAccept:
        parser->Accept();
        break;
    default:
        return LrError;
    }

    return action.Type;
}

LrActionType LrParser::Consume(LrParser *parser, int symbol, void *context)
{
    LrActionType action;

    while (true)
    {
        action = Step(parser, symbol, context);

        if (action != LrReduce)
            return action;
    }
}
