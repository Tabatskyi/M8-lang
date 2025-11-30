#include "BoolLiteralNode.hpp"

BoolLiteralNode::BoolLiteralNode(bool value)
    : _value(value) {}

bool BoolLiteralNode::value() const
{
    return _value;
}

void BoolLiteralNode::accept(ASTVisitor& visitor) const
{
    visitor.visitBoolLiteral(*this);
}
