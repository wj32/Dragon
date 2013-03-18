#pragma once

#include "Token.h"

#define LEXER_ALLOW_LINE_COMMENTS 0x1
#define LEXER_ALLOW_DOLLAR_SIGN_IN_ID 0x2

enum LexerState
{
    LsInvalid = 0,
    LsWs,
    LsWsWithLine,
    LsPpLine,
    LsSymbol1,
    LsSymbol2,
    LsId,
    LsWidePrefix,
    LsChar,
    LsCharEscape,
    LsString,
    LsStringEscape,
    LsNumber,
    LsExponent,
    LsExponentSign,
    LsNumberSuffix,
    LsComment,
    LsCommentStar,
    LsLineComment,
    LsMaximumLexerState
};

enum LexerImmediateState
{
    LiContinue,
    LiContinueAndIgnore,
    LiAccept,
    LiAcceptAndRetreat,
    LiError,
    LiEndOfFile
};

class Lexer
{
public:
    Lexer(std::istream &stream) :
        _stream(stream),
        _state(LsInvalid),
        _flagsInt(0),
        _options(0),
        _currentChar(0),
        _retreatChar(0),
        _symbol1(0),
        _symbol2(0)
    {
        Initialize();
    }

    ~Lexer(void);

    void Initialize();
    unsigned int GetOptions();
    void SetOptions(unsigned int options);
    bool GetToken(Token &token);
    bool GetError(std::string &errorMessage);
    bool IsEndOfFile();

private:
    typedef LexerImmediateState (Lexer::*StateFunction)(char c);

    static StateFunction _stateFunctions[LsMaximumLexerState];

    std::istream &_stream;
    std::string _errorMessage;
    LexerState _state;
    std::unordered_map<std::string, TokenId> _keywordMap;
    std::string _text;
    Token _token;
    union
    {
        struct
        {
            bool Error : 1;
            bool GlobalBackslash : 1;
            bool EndOfFile : 1;
            bool Retreat : 1;
            bool Spare1 : 4;
            bool Spare2 : 8;
            bool Spare3 : 8;
            bool Spare4 : 8;
        } _flags;
        int _flagsInt;
    };
    unsigned int _options;
    char _currentChar;
    char _retreatChar;
    char _symbol1;
    char _symbol2;
    int _lineNumber;

    void CreateKeywordMap();
    char GetChar();
    void RetreatChar();
    void SetError(std::string &errorMessage);
    void SetError(const char *errorMessage);
    void SetState(LexerState newState);

    LexerImmediateState StateLsInvalid(char c);
    LexerImmediateState StateLsWs(char c);
    LexerImmediateState StateLsWsWithLine(char c);
    LexerImmediateState StateLsPpLine(char c);
    LexerImmediateState StateLsSymbol1(char c);
    LexerImmediateState StateLsSymbol2(char c);
    LexerImmediateState StateLsId(char c);
    LexerImmediateState StateLsWidePrefix(char c);
    LexerImmediateState StateLsChar(char c);
    LexerImmediateState StateLsCharEscape(char c);
    LexerImmediateState StateLsString(char c);
    LexerImmediateState StateLsStringEscape(char c);
    LexerImmediateState StateLsNumber(char c);
    LexerImmediateState StateLsExponent(char c);
    LexerImmediateState StateLsExponentSign(char c);
    LexerImmediateState StateLsNumberSuffix(char c);
    LexerImmediateState StateLsComment(char c);
    LexerImmediateState StateLsCommentStar(char c);
    LexerImmediateState StateLsLineComment(char c);
    LexerImmediateState TransitionWs(char c);
    void SetToken(TokenId type);
    void AcceptToken(Token &token);
};
