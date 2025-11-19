#include "IDNode.hpp"

#include <utility>

#include "ASTVisitor.hpp"

IDNode::IDNode(std::string name)
    : _name(std::move(name)) {}

const std::string& IDNode::name() const
{
    return _name;
}

SymbolID IDNode::symbolId() const
{
    return _symbolId;
}

void IDNode::setSymbolId(SymbolID id) const
{
    _symbolId = id;
}

void IDNode::accept(ASTVisitor& visitor) const
{
    visitor.visitID(*this);
}
