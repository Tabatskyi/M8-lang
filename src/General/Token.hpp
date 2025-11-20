#pragma once

#include <string>
#include <vector>

enum class TokenType
{
    EndOfFile,
    Newline,
    Identifier,
    Number,
    StmtSep, // 'ᛵ'

    // Keywords / statements
    Var, // 'ᚡ'
    Const, // 'ᛍ'
    Return, // 'ᚷ'
    If, // 'ᛗ'
    Else, // 'ᛎ'
    Then, // 'ᛜ' 

    // Types 
    I32, // 'ᛰ'
    I64, // 'ᛯ'
    Bool, // 'ᛨ'

    // Literals
    True, // 'ᛉ'
    False, // 'ᛣ'

    // Operators
    Assign, // '᛬'
    Equals, // '᛬᛬'
    NotEqual, // 'ᛅ᛬'
    Not, // unary 'ᛅ'
    Add, // '᛭'
    Sub, // 'ᛧ'
    Mul, // '᛫'
    Div, // 'ᛇ'

    // Grouping
    LParen, // 'ᚮ'
    RParen, // 'ᚭ'

    // Logical ops
    And, // 'ᛤ'
    Or, // 'ᚢ'
    Xor, // 'ᛡ'

    // Compound assignment operators
    AddAssign, // '᛭᛬'
    SubAssign, // 'ᛧ᛬'
    MulAssign, // '᛫᛬'
    DivAssign, // 'ᛇ᛬'

	Function, // 'ᚠ'
    Struct, // 'ᛋ'
	Method, // 'ᛃ'

    // I/O
    Read, // 'ᚱ'
    Write, // 'ᚹ'

    // Checked ops
    CheckedAdd, // 'ꑭ᛭'
    CheckedSub, // 'ꑭᛧ'
    CheckedMul, // 'ꑭ᛫'
    CheckedDiv, // 'ꑭᛇ'
};

struct Token
{
    std::string lexeme;
    TokenType type;
};