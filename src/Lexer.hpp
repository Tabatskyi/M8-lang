#pragma once 

#include "Token.hpp"
#include <unordered_map>
#include <iostream>

using std::string;

constexpr const int ONE_CHAR_BYTES = 3;

static std::unordered_map<string, TokenType> doubleOperatorMap =
{
    {"᛬᛬", TokenType::Equals},
    {"ᛅ᛬", TokenType::NotEqual},
    {"᛭᛬", TokenType::AddAssign},
    {"ᛧ᛬", TokenType::SubAssign},
    {"᛫᛬", TokenType::MulAssign},
    {"ᛇ᛬", TokenType::DivAssign},
};

static std::unordered_map<string, TokenType> singleOperatorMap =
{
    {"ᛵ", TokenType::StmtSep},
    {"ᛜ", TokenType::Then},
    {"᛬", TokenType::Assign},
    {"᛭", TokenType::Add},
    {"ᛧ", TokenType::Sub},
    {"᛫", TokenType::Mul},
    {"ᛇ", TokenType::Div},
    {"ᛅ", TokenType::Not},
    {"ᚮ", TokenType::LParen},
    {"ᚭ", TokenType::RParen},
};

static std::unordered_map<string, TokenType> keywordMap =
{
    {"ᛗ", TokenType::If},
    {"ᛎ", TokenType::Else},
    {"ᛰ", TokenType::I32},
    {"ᛯ", TokenType::I64},
    {"ᛨ", TokenType::Bool},
    {"ᚷ", TokenType::Return},
    {"ᚡ", TokenType::Var},
    {"ᛍ", TokenType::Const},
    {"ᛉ", TokenType::True},
    {"ᛣ", TokenType::False},
    {"ᚠ", TokenType::Function},
    {"ᛋ", TokenType::Struct},
    {"ᛃ", TokenType::Method},
};

class Lexer
{
public:
	std::vector<Token> tokenize(const string& source) const;
};