#include "StringLiteralNode.hpp"

StringLiteralNode::StringLiteralNode(std::string value)
    : _value(std::move(value))
{
}

const std::string& StringLiteralNode::value() const
{
    return _value;
}

void StringLiteralNode::accept(ASTVisitor& visitor) const
{
    visitor.visitStringLiteral(*this);
}
