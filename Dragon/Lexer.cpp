#include "stdafx.h"
#include "Lexer.h"

#define NOTHING
#define STATE_FUNCTION(Name) &Lexer::State##Name

Lexer::StateFunction Lexer::_stateFunctions[LsMaximumLexerState] =
{
    STATE_FUNCTION(LsInvalid),
    STATE_FUNCTION(LsWs),
    STATE_FUNCTION(LsWsWithLine),
    STATE_FUNCTION(LsPpLine),
    STATE_FUNCTION(LsSymbol1),
    STATE_FUNCTION(LsSymbol2),
    STATE_FUNCTION(LsId),
    STATE_FUNCTION(LsWidePrefix),
    STATE_FUNCTION(LsChar),
    STATE_FUNCTION(LsCharEscape),
    STATE_FUNCTION(LsString),
    STATE_FUNCTION(LsStringEscape),
    STATE_FUNCTION(LsNumber),
    STATE_FUNCTION(LsExponent),
    STATE_FUNCTION(LsExponentSign),
    STATE_FUNCTION(LsNumberSuffix),
    STATE_FUNCTION(LsComment),
    STATE_FUNCTION(LsCommentStar),
    STATE_FUNCTION(LsLineComment)
};

Lexer::~Lexer()
{
    NOTHING;
}

void Lexer::Initialize()
{
    _state = LsWsWithLine;
    _lineNumber = 1;
    CreateKeywordMap();
}

unsigned int Lexer::GetOptions()
{
    return _options;
}

void Lexer::SetOptions(unsigned int options)
{
    _options = options;
    CreateKeywordMap();
}

bool Lexer::GetToken(Token &token)
{
    LexerImmediateState immediate;
    StateFunction stateFunction;
    char c;

    if (IsEndOfFile())
        return false;
    if (_flags.Error)
        return false;

    while (true)
    {
        c = GetChar();

        if (!_flags.GlobalBackslash)
        {
            if (c != '\\' ||
                _state == LsString || _state == LsStringEscape ||
                _state == LsChar || _state == LsCharEscape ||
                _state == LsComment || _state == LsCommentStar || _state == LsLineComment)
            {
                assert(_state < LsMaximumLexerState);
                stateFunction = _stateFunctions[_state];
                immediate = (this->*stateFunction)(c);

                switch (immediate)
                {
                case LiContinue:
                    if (c != 0)
                        _text += c;
                    break;
                case LiContinueAndIgnore:
                    break;
                case LiAccept:
                    if (c != 0)
                        _text += c;
                    AcceptToken(token);
                    _text.clear();
                    SetState(LsWs);
                    return true;
                case LiAcceptAndRetreat:
                    AcceptToken(token);
                    _text.clear();
                    RetreatChar();
                    SetState(LsWs);
                    return true;
                case LiError:
                    _flags.Error = true;
                    return false;
                case LiEndOfFile:
                    return false;
                }
            }
            else
            {
                _flags.GlobalBackslash = true;
            }
        }
        else
        {
            if (c == '\n')
            {
                NOTHING;
            }
            else if (c == 0)
            {
                RetreatChar();
            }
            else if (_state != LsPpLine)
            {
                std::string message = "invalid escape sequence: '\\";

                message += c;
                message += '\'';
                SetError(message);
                _flags.Error = true;
            }

            _flags.GlobalBackslash = false;
        }
    }
}

bool Lexer::GetError(std::string &errorMessage)
{
    if (_flags.Error)
    {
        errorMessage = _errorMessage;
        return true;
    }
    else
    {
        return false;
    }
}

bool Lexer::IsEndOfFile()
{
    return _flags.EndOfFile;
}

void Lexer::CreateKeywordMap()
{
    _keywordMap.clear();
    _keywordMap["auto"] = WordAutoType;
    _keywordMap["break"] = WordBreakType;
    _keywordMap["case"] = WordCaseType;
    _keywordMap["char"] = WordCharType;
    _keywordMap["const"] = WordConstType;
    _keywordMap["continue"] = WordContinueType;
    _keywordMap["default"] = WordDefaultType;
    _keywordMap["do"] = WordDoType;
    _keywordMap["double"] = WordDoubleType;
    _keywordMap["else"] = WordElseType;
    _keywordMap["enum"] = WordEnumType;
    _keywordMap["extern"] = WordExternType;
    _keywordMap["float"] = WordFloatType;
    _keywordMap["for"] = WordForType;
    _keywordMap["goto"] = WordGotoType;
    _keywordMap["if"] = WordIfType;
    _keywordMap["int"] = WordIntType;
    _keywordMap["long"] = WordLongType;
    _keywordMap["register"] = WordRegisterType;
    _keywordMap["return"] = WordReturnType;
    _keywordMap["short"] = WordShortType;
    _keywordMap["signed"] = WordSignedType;
    _keywordMap["sizeof"] = WordSizeofType;
    _keywordMap["static"] = WordStaticType;
    _keywordMap["struct"] = WordStructType;
    _keywordMap["switch"] = WordSwitchType;
    _keywordMap["typedef"] = WordTypedefType;
    _keywordMap["union"] = WordUnionType;
    _keywordMap["unsigned"] = WordUnsignedType;
    _keywordMap["void"] = WordVoidType;
    _keywordMap["volatile"] = WordVolatileType;
    _keywordMap["while"] = WordWhileType;
}

char Lexer::GetChar()
{
    char c;

    if (_flags.Retreat)
    {
        c = _retreatChar;
        _retreatChar = 0;
        _flags.Retreat = false;
    }
    else
    {
        if (_flags.EndOfFile)
            return 0;

        _stream.get(c);

        if (_stream.eof())
        {
            _flags.EndOfFile = true;
            c = 0;
        }
    }

    _currentChar = c;

    return c;
}

void Lexer::RetreatChar()
{
    _retreatChar = _currentChar;
    _flags.Retreat = true;
}

void Lexer::SetError(std::string &errorMessage)
{
    _flags.Error = true;
    _errorMessage = errorMessage;
}

void Lexer::SetError(const char *errorMessage)
{
    _flags.Error = true;
    _errorMessage = errorMessage;
}

void Lexer::SetState(LexerState newState)
{
    _state = newState;
}

LexerImmediateState Lexer::StateLsInvalid(char c)
{
    assert(false);
    SetError("LsInvalid reached");
    return LiError;
}

LexerImmediateState Lexer::StateLsWs(char c)
{
    if (c == '\n')
        _lineNumber++;

    if (c == ' ' || c == '\t' || c == '\v' || c == '\f' || c == '\r')
        return LiContinueAndIgnore;

    if (c == '\n')
    {
        SetState(LsWsWithLine);
        return LiContinueAndIgnore;
    }

    return TransitionWs(c);
}

LexerImmediateState Lexer::StateLsWsWithLine(char c)
{
    if (c == '\n')
        _lineNumber++;

    if (c == ' ' || c == '\t' || c == '\v' || c == '\f' || c == '\r' || c == '\n')
        return LiContinueAndIgnore;

    if (c == '#')
    {
        SetState(LsPpLine);
        return LiContinue;
    }

    return TransitionWs(c);
}

LexerImmediateState Lexer::StateLsPpLine(char c)
{
    if (c == '\n')
    {
        SetToken(PreLineType);
        return LiAcceptAndRetreat;
    }
    else if (c == 0)
    {
        SetToken(PreLineType);
        return LiAccept;
    }
    else
    {
        return LiContinue;
    }
}

LexerImmediateState Lexer::StateLsSymbol1(char c)
{
    _symbol2 = 0;

    switch (_symbol1)
    {
    case '.':
        if (c == '.')
        {
            _symbol2 = '.';
            SetState(LsSymbol2);
            return LiContinue;
        }
        else
        {
            SetToken(OpDotType);
            return LiAcceptAndRetreat;
        }
        break;
    case '=':
        if (c == '=')
        {
            SetToken(OpEqualsEqualsType);
            return LiAccept;
        }
        else
        {
            SetToken(OpEqualsType);
            return LiAcceptAndRetreat;
        }
        break;
    case '+':
        if (c == '=')
        {
            SetToken(OpPlusEqualsType);
            return LiAccept;
        }
        else if (c == '+')
        {
            SetToken(OpPlusPlusType);
            return LiAccept;
        }

        SetToken(OpPlusType);
        return LiAcceptAndRetreat;
    case '-':
        if (c == '=')
        {
            SetToken(OpMinusEqualsType);
            return LiAccept;
        }
        else if (c == '-')
        {
            SetToken(OpMinusType);
            return LiAccept;
        }
        else if (c == '>')
        {
            SetToken(OpMinusGreaterType);
            return LiAccept;
        }

        SetToken(OpMinusType);
        return LiAcceptAndRetreat;
    case '*':
        if (c == '=')
        {
            SetToken(OpStarEqualsType);
            return LiAccept;
        }

        SetToken(OpStarType);
        return LiAcceptAndRetreat;
    case '/':
        if (c == '=')
        {
            SetToken(OpSlashEqualsType);
            return LiAccept;
        }
        else if (c == '*')
        {
            SetState(LsComment);
            return LiContinue;
        }
        else if (c == '/' && (_options & LEXER_ALLOW_LINE_COMMENTS))
        {
            SetState(LsLineComment);
            return LiContinue;
        }

        SetToken(OpSlashType);
        return LiAcceptAndRetreat;
    case '%':
        if (c == '=')
        {
            SetToken(OpPercentEqualsType);
            return LiAccept;
        }

        SetToken(OpPercentType);
        return LiAcceptAndRetreat;
    case '<':
        if (c == '=')
        {
            SetToken(OpLessEqualsType);
            return LiAccept;
        }
        else if (c == '<')
        {
            _symbol2 = '<';
            SetState(LsSymbol2);
            return LiContinue;
        }

        SetToken(OpLessType);
        return LiAcceptAndRetreat;
    case '>':
        if (c == '=')
        {
            SetToken(OpGreaterEqualsType);
            return LiAccept;
        }
        else if (c == '>')
        {
            _symbol2 = '>';
            SetState(LsSymbol2);
            return LiContinue;
        }

        SetToken(OpGreaterType);
        return LiAcceptAndRetreat;
    case '!':
        if (c == '=')
        {
            SetToken(OpBangEqualsType);
            return LiAccept;
        }

        SetToken(OpBangType);
        return LiAcceptAndRetreat;
    case '&':
        if (c == '=')
        {
            SetToken(OpAndEqualsType);
            return LiAccept;
        }
        else if (c == '&')
        {
            SetToken(OpAndAndType);
            return LiAccept;
        }

        SetToken(OpAndType);
        return LiAcceptAndRetreat;
    case '|':
        if (c == '=')
        {
            SetToken(OpOrEqualsType);
            return LiAccept;
        }
        else if (c == '|')
        {
            SetToken(OpOrOrType);
            return LiAccept;
        }

        SetToken(OpOrType);
        return LiAcceptAndRetreat;
    case '^':
        if (c == '=')
        {
            SetToken(OpCaretEqualsType);
            return LiAccept;
        }

        SetToken(OpCaretType);
        return LiAcceptAndRetreat;
    default:
        assert(false);
        SetError("LsSymbol1 reached with invalid symbol");
        return LiError;
    }
}

LexerImmediateState Lexer::StateLsSymbol2(char c)
{
    switch (_symbol2)
    {
    case '.':
        assert(_symbol1 == '.');

        if (c == '.')
        {
            SetToken(SymDotDotDotType);
            return LiAccept;
        }

        SetToken(SymDotDotType);
        return LiAcceptAndRetreat;
    case '<':
        assert(_symbol1 == '<');

        if (c == '=')
        {
            SetToken(OpLessLessEqualsType);
            return LiAccept;
        }

        SetToken(OpLessLessType);
        return LiAcceptAndRetreat;
    case '>':
        assert(_symbol1 == '>');

        if (c == '=')
        {
            SetToken(OpGreaterGreaterEqualsType);
            return LiAccept;
        }

        SetToken(OpGreaterGreaterType);
        return LiAcceptAndRetreat;
    default:
        assert(false);
        SetError("LsSymbol2 reached with invalid symbol");
        return LiError;
    }
}

LexerImmediateState Lexer::StateLsId(char c)
{
    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_')
    {
        return LiContinue;
    }
    else if (c == '$' && (_options & LEXER_ALLOW_DOLLAR_SIGN_IN_ID))
    {
        return LiContinue;
    }
    else
    {
        SetToken(IdType);
        return LiAcceptAndRetreat;
    }
}

LexerImmediateState Lexer::StateLsWidePrefix(char c)
{
    if (c == 0)
    {
        SetToken(IdType);
        return LiAccept;
    }
    else if (c == '\'')
    {
        SetState(LsChar);
        return LiContinue;
    }
    else if (c == '"')
    {
        SetState(LsString);
        return LiContinue;
    }
    else if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_')
    {
        SetState(LsId);
        return LiContinue;
    }
    else if (c == '$' && (_options & LEXER_ALLOW_DOLLAR_SIGN_IN_ID))
    {
        SetState(LsId);
        return LiContinue;
    }
    else
    {
        SetToken(IdType);
        return LiAcceptAndRetreat;
    }
}

LexerImmediateState Lexer::StateLsChar(char c)
{
    if (c == '\'')
    {
        SetToken(LiteralCharType);
        return LiAccept;
    }
    else if (c == '\\')
    {
        SetState(LsCharEscape);
        return LiContinue;
    }
    else if (c == 0)
    {
        SetError("unexpected end-of-file in literal");
        return LiError;
    }
    else
    {
        return LiContinue;
    }
}

LexerImmediateState Lexer::StateLsCharEscape(char c)
{
    if (c == 0)
    {
        SetError("unexpected end-of-file in literal");
        return LiError;
    }

    SetState(LsChar);
    return LiContinue;
}

LexerImmediateState Lexer::StateLsString(char c)
{
    if (c == '"')
    {
        SetToken(LiteralStringType);
        return LiAccept;
    }
    else if (c == '\\')
    {
        SetState(LsStringEscape);
        return LiContinue;
    }
    else if (c == 0)
    {
        SetError("unexpected end-of-file in literal");
        return LiError;
    }
    else
    {
        return LiContinue;
    }
}

LexerImmediateState Lexer::StateLsStringEscape(char c)
{
    if (c == 0)
    {
        SetError("unexpected end-of-file in literal");
        return LiError;
    }

    SetState(LsString);
    return LiContinue;
}

LexerImmediateState Lexer::StateLsNumber(char c)
{
    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'd') || (c >= 'A' && c <= 'D') ||
        c == 'f' || c == 'F' || c == 'x' || c == 'X' || c == '.')
    {
        return LiContinue;
    }
    else if (c == 'e' || c == 'E')
    {
        SetState(LsExponent);
        return LiContinue;
    }
    else if (c == 'l' || c == 'L' || c == 'u' || c == 'U')
    {
        SetState(LsNumberSuffix);
        return LiContinue;
    }
    else
    {
        SetToken(LiteralNumberType);
        return LiAcceptAndRetreat;
    }
}

LexerImmediateState Lexer::StateLsExponent(char c)
{ 
    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') ||
        c == '+' || c == '-')
    {
        SetState(LsExponentSign);
        return LiContinue;
    }
    else if (c == 'l' || c == 'L' || c == 'u' || c == 'U')
    {
        SetState(LsNumberSuffix);
        return LiContinue;
    }
    else
    {
        SetToken(LiteralNumberType);
        return LiAcceptAndRetreat;
    }
}

LexerImmediateState Lexer::StateLsExponentSign(char c)
{
    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
    {
        return LiContinue;
    }
    else if (c == 'l' || c == 'L' || c == 'u' || c == 'U')
    {
        SetState(LsNumberSuffix);
        return LiContinue;
    }
    else
    {
        SetToken(LiteralNumberType);
        return LiAcceptAndRetreat;
    }
}

LexerImmediateState Lexer::StateLsNumberSuffix(char c)
{
    if (c == 'l' || c == 'L' || c == 'u' || c == 'U')
    {
        return LiContinue;
    }
    else
    {
        SetToken(LiteralNumberType);
        return LiAcceptAndRetreat;
    }
}

LexerImmediateState Lexer::StateLsComment(char c)
{
    if (c == '*')
    {
        SetState(LsCommentStar);
        return LiContinue;
    }
    else if (c == 0)
    {
        SetError("unexpected end-of-file in comment");
        return LiError;
    }
    else
    {
        return LiContinue;
    }
}

LexerImmediateState Lexer::StateLsCommentStar(char c)
{
    if (c == '/')
    {
        SetToken(CommentType);
        return LiAccept;
    }
    else if (c == 0)
    {
        SetError("unexpected end-of-file in comment");
        return LiError;
    }
    else
    {
        SetState(LsComment);
        return LiContinue;
    }
}

LexerImmediateState Lexer::StateLsLineComment(char c)
{
    if (c == '\n')
    {
        SetToken(LineCommentType);
        return LiAcceptAndRetreat;
    }
    else if (c == 0)
    {
        SetToken(LineCommentType);
        return LiAccept;
    }
    else
    {
        return LiContinue;
    }
}

LexerImmediateState Lexer::TransitionWs(char c)
{
    if (c == 0)
    {
        return LiEndOfFile;
    }
    else if (c >= '0' && c <= '9')
    {
        SetState(LsNumber);
        return LiContinue;
    }
    else if (c == 'L')
    {
        SetState(LsWidePrefix);
        return LiContinue;
    }
    else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_')
    {
        SetState(LsId);
        return LiContinue;
    }
    else if (c == '$' && (_options & LEXER_ALLOW_DOLLAR_SIGN_IN_ID))
    {
        SetState(LsId);
        return LiContinue;
    }
    else if (c == '\'')
    {
        SetState(LsChar);
        return LiContinue;
    }
    else if (c == '\"')
    {
        SetState(LsString);
        return LiContinue;
    }
    else
    {
        LexerImmediateState immediate;

        _symbol1 = 0;
        immediate = LiError;

        switch (c)
        {
        case '(':
            SetToken(SymLeftRoundType);
            return LiAccept;
        case ')':
            SetToken(SymRightRoundType);
            return LiAccept;
        case '[':
            SetToken(SymLeftSquareType);
            return LiAccept;
        case ']':
            SetToken(SymRightSquareType);
            return LiAccept;
        case '{':
            SetToken(SymLeftCurlyType);
            return LiAccept;
        case '}':
            SetToken(SymRightCurlyType);
            return LiAccept;
        case ';':
            SetToken(SymSemicolonType);
            return LiAccept;
        case '?':
            SetToken(SymQuestionType);
            return LiAccept;
        case ':':
            SetToken(SymColonType);
            return LiAccept;
        case '~':
            SetToken(OpTildeType);
            return LiAccept;
        case ',':
            SetToken(OpCommaType);
            return LiAccept;
        case '.':
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
            _symbol1 = c;
            SetState(LsSymbol1);
            return LiContinue;
        default:
            std::string message = "unexpected symbol '";
            message += c;
            message += '\'';
            SetError(message);
            return LiError;
        }
    }
}

void Lexer::SetToken(TokenId type)
{
    _token.Type = type;
    _token.Text.clear();
    _token.LineNumber = _lineNumber;
}

void Lexer::AcceptToken(Token &token)
{
    if (_token.Type == IdType)
    {
        // Convert Id tokens to Word tokens.

        auto it = _keywordMap.find(_text);

        if (it != _keywordMap.end())
        {
            _token.Type = it->second;
        }
    }

    token = _token;
    token.Text = _text;

    _token.Type = NoneType;
    _token.Text.clear();
}
