#include "FieldAccessNode.hpp"

FieldAccessNode::FieldAccessNode(std::string base, std::vector<std::string> chain)
    : _base(std::move(base)), _fields(std::move(chain)) {}

const std::string& FieldAccessNode::base() const
{
    return _base;
}

const std::vector<std::string>& FieldAccessNode::fieldChain() const
{
    return _fields;
}

SymbolID FieldAccessNode::baseSymbolId() const
{
    return _baseId;
}

void FieldAccessNode::setBaseSymbolId(SymbolID id) const
{
    _baseId = id;
}

void FieldAccessNode::accept(ASTVisitor& visitor) const
{
    visitor.visitFieldAccess(*this);
}
