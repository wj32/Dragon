#pragma once

// Symbols marked with an asterisk are prefixes of other tokens.

enum TokenId
{
    NoneType = 0,
    PreLineType = 1,
    IdType = 2,
    LiteralNumberType = 3,
    LiteralCharType = 4,
    LiteralStringType = 5,
    CommentType = 6,
    LineCommentType = 7,
    LowerWordType = 10,
    WordAutoType,
    WordBreakType,
    WordCaseType,
    WordCharType,
    WordConstType,
    WordContinueType,
    WordDefaultType,
    WordDoType,
    WordDoubleType,
    WordElseType,
    WordEnumType,
    WordExternType,
    WordFloatType,
    WordForType,
    WordGotoType,
    WordIfType,
    WordIntType,
    WordLongType,
    WordRegisterType,
    WordReturnType,
    WordShortType,
    WordSignedType,
    WordSizeofType,
    WordStaticType,
    WordStructType,
    WordSwitchType,
    WordTypedefType,
    WordUnionType,
    WordUnsignedType,
    WordVoidType,
    WordVolatileType,
    WordWhileType,
    UpperWordType = 1000,
    LowerSymType = 1000,
    SymLeftRoundType,
    SymRightRoundType,
    SymLeftSquareType,
    SymRightSquareType,
    SymLeftCurlyType,
    SymRightCurlyType,
    SymSemicolonType,
    SymQuestionType,
    SymColonType,
    SymDotDotType,
    SymDotDotDotType,
    UpperSymType = 100000,
    LowerOpType = 100000,
    OpEqualsType, // *
    OpPlusType, // *
    OpMinusType, // *
    OpStarType, // *
    OpSlashType, // *
    OpPercentType, // *
    OpPlusPlusType,
    OpMinusMinusType,
    OpEqualsEqualsType,
    OpBangEqualsType,
    OpLessType, // *
    OpGreaterType, // *
    OpLessEqualsType,
    OpGreaterEqualsType,
    OpBangType, // *
    OpAndAndType,
    OpOrOrType,
    OpTildeType,
    OpAndType, // *
    OpOrType, // *
    OpCaretType, // *
    OpLessLessType, // *
    OpGreaterGreaterType, // *
    OpPlusEqualsType,
    OpMinusEqualsType,
    OpStarEqualsType,
    OpSlashEqualsType,
    OpPercentEqualsType,
    OpAndEqualsType,
    OpOrEqualsType,
    OpCaretEqualsType,
    OpLessLessEqualsType,
    OpGreaterGreaterEqualsType,
    OpDotType, // *
    OpMinusGreaterType,
    OpCommaType,
    UpperOpType = 1000000
};

struct Token
{
    TokenId Type;
    std::string Text;
    int LineNumber;
};
