#pragma once

#include "../General/Utility.hpp" // brings in ValueType, SymbolID, InvalidSymbolID

#include <string>
#include <utility>

struct TypeDesc
{
    enum class Kind { Builtin, Struct };

    Kind kind = Kind::Builtin;
    ValueType builtin = ValueType::Invalid;
    std::string structName;

    static TypeDesc Builtin(ValueType t)
    {
        TypeDesc d;
        d.kind = Kind::Builtin;
        d.builtin = t;
        return d;
    }

    static TypeDesc Struct(std::string name)
    {
        TypeDesc d;
        d.kind = Kind::Struct;
        d.structName = std::move(name);
        return d;
    }
};