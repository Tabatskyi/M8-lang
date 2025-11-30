#include "AssignNode.hpp"

AssignNode::AssignNode(std::string identifier, std::unique_ptr<ExprNode> value)
    : _identifier(std::move(identifier)), _value(std::move(value))
{
}

const std::string& AssignNode::identifier() const
{
    return _identifier;
}

const ExprNode* AssignNode::value() const
{
    return _value.get();
}

SymbolID AssignNode::symbolId() const
{
    return _symbolId;
}

void AssignNode::setSymbolId(SymbolID id) const
{
    _symbolId = id;
}

void AssignNode::accept(ASTVisitor& visitor) const
{
    visitor.visitAssign(*this);
}