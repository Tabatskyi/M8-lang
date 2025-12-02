#pragma once

#include <string>
#include <unordered_map>

enum class ValueType
{
    Invalid,
    I32,
    I64,
    Bool,
    String
};

using SymbolID = size_t;
constexpr SymbolID InvalidSymbolID = static_cast<SymbolID>(-1);

inline bool isNumeric(ValueType type)
{
    return type == ValueType::I32 || type == ValueType::I64;
}

inline ValueType widerType(ValueType leftHandSide, ValueType rightHandSide)
{
    if (!isNumeric(leftHandSide) || !isNumeric(rightHandSide))
        return ValueType::Invalid;
    if (leftHandSide == ValueType::I64 || rightHandSide == ValueType::I64)
        return ValueType::I64;
    return ValueType::I32;
}

inline ValueType comparisonOperandType(ValueType leftHandSide, ValueType rightHandSide)
{
    if (leftHandSide == ValueType::Bool && rightHandSide == ValueType::Bool)
        return ValueType::Bool;
    if (leftHandSide == ValueType::String && rightHandSide == ValueType::String)
        return ValueType::String;
    return widerType(leftHandSide, rightHandSide);
}

inline bool isAssignable(ValueType target, ValueType source)
{
    if (target == ValueType::Invalid || source == ValueType::Invalid)
        return false;
    if (target == source)
        return true;
    if (target == ValueType::I64 && source == ValueType::I32)
        return true;
    return false;
}

inline bool canConvertToI32(ValueType type)
{
    return type == ValueType::I32 || type == ValueType::I64 || type == ValueType::Bool;
}

inline std::string typeToString(ValueType type)
{
    switch (type)
    {
    case ValueType::I32: return "i32";
    case ValueType::I64: return "i64";
    case ValueType::Bool: return "bool";
    case ValueType::String: return "string";
    default: return "<invalid>";
    }
}