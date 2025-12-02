#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "../AST/TypeDesc.hpp"
#include "../General/Utility.hpp"

struct VariableInfo
{
    TypeDesc type{ TypeDesc::Builtin(ValueType::Invalid) };
    bool isMutable = false;
    std::string name;
    size_t scopeId = 0;
};

struct StructFieldInfo
{
    TypeDesc type{ TypeDesc::Builtin(ValueType::Invalid) };
    bool isMutable = false;
    std::string name;
};

struct StructInfo
{
    std::string name;
    std::vector<StructFieldInfo> fields;
};

struct FunctionParamInfo
{
    TypeDesc type{ TypeDesc::Builtin(ValueType::Invalid) };
    std::string name;
    SymbolID symbolId = InvalidSymbolID;
};

struct FunctionInfo
{
    std::string name;
    TypeDesc returnType{ TypeDesc::Builtin(ValueType::Invalid) };
    std::vector<FunctionParamInfo> params;
    size_t scopeId = 0;
    bool isMember = false;
    std::string masterStruct;
    bool isBuiltin = false;
};

using StructTable = std::unordered_map<std::string, StructInfo>;
using FunctionTable = std::unordered_map<std::string, FunctionInfo>;