#include "stdafx.h"
#include "Lexer.h"
#include "LrGenerator.h"
#include "CParser.h"
#include "IcDump.h"
#include "IcGraph.h"
#include <fstream>

void Dirty();
void InteractiveLexer();
int GenerateTable();
void TestParser(char *definitionFile, char *tableFile, char *inputFile, bool wait);

#ifdef __GNUC__
int wmain(int argc, wchar_t *argv[]);

int main(int argc, char *argv[])
{
    int wargc;
    wchar_t **wargv;

    wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    return wmain(wargc, wargv);
}
#endif

int wmain(int argc, wchar_t *argv[])
{
    if (argc > 1)
    {
        if (wcscmp(argv[1], L"dirty") == 0)
        {
            Dirty();
        }
        else if (wcscmp(argv[1], L"token") == 0)
        {
            InteractiveLexer();
        }
        else if (wcscmp(argv[1], L"lrgen") == 0)
        {
            return GenerateTable();
        }
        else
        {
            std::wcerr << L"Invalid mode '" << argv[1] << L"'" << std::endl;
        }
    }
    else
    {
        //TestParser("D:\\projects\\Dragon\\Dragon\\Definition.txt", "D:\\projects\\Dragon\\Dragon\\Definition.table", "D:\\projects\\Dragon\\Dragon\\test.c", true);
        TestParser("Definition.txt", "Definition.table", "test.c", false);
    }

    return 0;
}

void Dirty()
{
    Lexer lexer(std::cin);
    Token token;
    std::string errorMessage;
    bool preNeedNewLine = false;
    bool idNeedSpace = false;
    bool intNeedSpace = false;
    bool opNeedSpace = false;

    lexer.SetOptions(lexer.GetOptions() | LEXER_ALLOW_LINE_COMMENTS | LEXER_ALLOW_DOLLAR_SIGN_IN_ID);

    while (lexer.GetToken(token))
    {
        if (token.Type > LowerWordType && token.Type < UpperWordType)
            token.Type = IdType; // keywords are identical to identifiers for our purposes

        switch (token.Type)
        {
        case PreLineType:
            if (preNeedNewLine)
                std::cout << '\n';
            std::cout << token.Text << '\n';
            preNeedNewLine = false;
            idNeedSpace = false;
            intNeedSpace = false;
            opNeedSpace = false;
            break;
        case IdType:
            if (idNeedSpace)
                std::cout << ' ';
            std::cout << token.Text;
            preNeedNewLine = true;
            idNeedSpace = true;
            intNeedSpace = true;
            opNeedSpace = false;
            break;
        case LiteralNumberType:
            if (intNeedSpace)
                std::cout << ' ';
            std::cout << token.Text;
            preNeedNewLine = true;
            idNeedSpace = true;
            intNeedSpace = true;
            opNeedSpace = false;
            break;
        case CommentType:
        case LineCommentType:
            break;
        default:
            std::cout << token.Text;

            // Operators that are prefixes of other operators might need to have a space appended.
            if (token.Type > LowerOpType && token.Type < UpperOpType)
            {
                if (opNeedSpace)
                    std::cout << ' ';

                if (token.Text.length() == 1)
                {
                    switch (token.Text[0])
                    {
                    case '=':
                    case '+':
                    case '-':
                    case '*':
                    case '/':
                    case '%':
                    case '<':
                    case '>':
                    case '!':
                    case '&':
                    case '|':
                    case '^':
                        opNeedSpace = true;
                        break;
                    }
                }
                else if (token.Text.length() == 2)
                {
                    if (token.Text[0] == '<' && token.Text[1] == '<')
                    {
                        opNeedSpace = true;
                    }
                    else if (token.Text[0] == '>' && token.Text[1] == '>')
                    {
                        opNeedSpace = true;
                    }
                }
            }
            preNeedNewLine = true;
            idNeedSpace = false;
            intNeedSpace = false;
            break;
        }
    }

    if (lexer.GetError(errorMessage))
    {
        std::cerr << "Error: " << errorMessage << std::endl;
    }
}

void InteractiveLexer()
{
    Lexer lexer(std::cin);
    Token token;
    std::string errorMessage;

    lexer.SetOptions(lexer.GetOptions() | LEXER_ALLOW_LINE_COMMENTS | LEXER_ALLOW_DOLLAR_SIGN_IN_ID);

    while (lexer.GetToken(token))
    {
        std::cout << "Token " << token.Type << ": " << token.Text << std::endl;
    }

    if (lexer.GetError(errorMessage))
    {
        std::cerr << "Error: " << errorMessage << std::endl;
    }

    getchar();
    getchar();
}

int GenerateTable()
{
    LrGenerator gen;
    std::ifstream stream("D:\\projects\\Dragon\\Dragon\\Definition.txt");
    std::ofstream tablestream("D:\\projects\\Dragon\\Dragon\\Definition.table", std::ios_base::out | std::ios_base::binary);
    LrTable *table;

    if (!gen.Load(stream))
    {
        std::string errorMessage;

        gen.GetError(errorMessage);
        std::cerr << errorMessage << std::endl;

        return 1;
    }

    table = gen.Generate();

    if (!table)
    {
        std::string errorMessage;

        gen.GetError(errorMessage);
        std::cerr << errorMessage << std::endl;

        return 1;
    }

    LrParser::SaveTable(tablestream, table);

    std::cerr << "Table size: " << table->size() << std::endl;

    for (auto it = table->begin(); it != table->end(); ++it)
    {
        std::cout << "(" << it->first.first << ", " << it->first.second << ") -> ";

        switch (it->second.Type)
        {
        case LrShift:
            std::cout << "SHIFT";
            break;
        case LrGoto:
            std::cout << "GOTO";
            break;
        case LrReduce:
            std::cout << "REDUCE";
            break;
        case LrAccept:
            std::cout << "ACCEPT";
            break;
        }

        std::cout << " " << it->second.Id << std::endl;
    }

    return 0;
}

void TestParser(char *definitionFile, char *tableFile, char *inputFile, bool wait)
{
    LrGenerator gen;
    std::ifstream stream(definitionFile);
    std::ifstream tablestream(tableFile, std::ios_base::in | std::ios_base::binary);
    std::ifstream cfile(inputFile);
    LrTable *table;
    CParser parser;
    IcObjectManager *objectManager;

    if (!gen.Load(stream))
    {
        std::string errorMessage;

        gen.GetError(errorMessage);
        std::cerr << errorMessage << std::endl;

        return;
    }

    table = LrParser::LoadTable(tablestream);

    if (!table)
    {
        return;
    }

    objectManager = new IcObjectManager;
    parser.Initialize(table, &gen, objectManager);

#ifdef NDEBUG
    try
    {
#endif
        if (!parser.Parse(cfile))
            throw "parse error";
#ifdef NDEBUG
    }
    catch (char *e)
    {
        std::cout << "Error near line " << parser.LastToken().LineNumber << ": " << e;
        if (wait)
            getchar();
        return;
    }
#endif

    IcDump dump(std::cout);
    auto objects = objectManager->GetObjects();

    for (auto it = objects.first; it != objects.second; ++it)
    {
        IcObject *object = *it;

        if (object->Type == IcNormalObjectType)
        {
            if (object->n.Function)
            {
                dump.DumpFunction(object->Name, object->n.Function);
                std::cout << std::endl;

                IcGraph forward;
                IcGraph backward;
                IcLabelMultimap dominators;
                IcLabelMap immediateDominators;

                IcGraph::CreateGraph(object->n.Function, forward, backward);
                IcGraph::RemoveUnreachable(object->n.Function, forward, backward);
                IcGraph::Dominators(object->n.Function, forward, backward, dominators);
                IcGraph::ImmediateDominators(object->n.Function, forward, backward, dominators, immediateDominators);

                for (auto it2 = dominators.begin(); it2 != dominators.end(); ++it2)
                {
                    dump.DumpLabel(object->n.Function->BlockMap[it2->first]);
                    std::cout << " (";
                    dump.DumpLabel(object->n.Function->BlockMap[immediateDominators[it2->first]]);

                    std::cout << "):";

                    for (auto it3 = it2->second.begin(); it3 != it2->second.end(); ++it3)
                    {
                        std::cout << " ";
                        dump.DumpLabel(object->n.Function->BlockMap[*it3]);
                    }

                    std::cout << std::endl;
                }
            }
        }
    }

    if (wait)
        getchar();

    return;
}
