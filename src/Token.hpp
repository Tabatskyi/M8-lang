#pragma once

#include <string>
#include <vector>

enum class TokenType
{
    EndOfFile,
    Newline,
    Identifier,
    Number,
    Var,
    Mut,
    Return,
    If,
    Else,
    I32,
    I64,
    Bool,
    True,
    False,
    BlockStart,
    BlockEnd,
    Assign,
    Equals,
    NotEqual,
    Not,
    Add,
    Sub,
    Mul
};

struct Token
{
    std::string lexeme;
    TokenType type;
};