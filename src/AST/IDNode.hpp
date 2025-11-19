#pragma once

#include <string>

#include "FactorNode.hpp"
#include "TypeDesc.hpp"

class IDNode : public FactorNode
{
public:
    explicit IDNode(std::string name);

    const std::string& name() const;
    SymbolID symbolId() const;
    void setSymbolId(SymbolID id) const;

    void accept(ASTVisitor& visitor) const override;

private:
    std::string _name;
    mutable SymbolID _symbolId = InvalidSymbolID;
};
