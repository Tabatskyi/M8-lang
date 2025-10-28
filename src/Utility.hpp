#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

enum class ValueType
{
    Invalid,
    I32,
    I64,
    Bool
};

using SymbolID = std::size_t;
constexpr SymbolID InvalidSymbolID = static_cast<SymbolID>(-1);

struct VariableInfo
{
    ValueType type = ValueType::Invalid;
    bool isMutable = false;
    std::string name;
    std::size_t scopeId = 0;
};

inline bool isNumeric(ValueType type)
{
    return type == ValueType::I32 || type == ValueType::I64;
}

inline ValueType widerType(ValueType lhs, ValueType rhs)
{
    if (!isNumeric(lhs) || !isNumeric(rhs))
        return ValueType::Invalid;
    if (lhs == ValueType::I64 || rhs == ValueType::I64)
        return ValueType::I64;
    return ValueType::I32;
}

inline ValueType comparisonOperandType(ValueType lhs, ValueType rhs)
{
    if (lhs == ValueType::Bool && rhs == ValueType::Bool)
        return ValueType::Bool;
    return widerType(lhs, rhs);
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
    default: return "<invalid>";
    }
}
