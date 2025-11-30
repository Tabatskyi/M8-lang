#pragma once

#include "../General/Utility.hpp"

#include <string>
#include <utility>

struct TypeDesc
{
    enum class Kind { Builtin, Struct, TemplateParam };

    Kind kind = Kind::Builtin;
    ValueType builtin = ValueType::Invalid;
    std::string structName;
    std::string templateName;

    static TypeDesc Builtin(ValueType type)
    {
        TypeDesc desc;
        desc.kind = Kind::Builtin;
        desc.builtin = type;
        return desc;
    }

    static TypeDesc Struct(std::string name)
    {
        TypeDesc desc;
        desc.kind = Kind::Struct;
        desc.structName = std::move(name);
        return desc;
    }

    static TypeDesc TemplateParam(std::string name)
    {
        TypeDesc desc;
        desc.kind = Kind::TemplateParam;
        desc.templateName = std::move(name);
        return desc;
    }
};