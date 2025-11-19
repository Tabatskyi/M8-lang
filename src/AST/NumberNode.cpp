#include "NumberNode.hpp"

#include "ASTVisitor.hpp"

NumberNode::NumberNode(std::int64_t value)
    : _value(value)
{
}

std::int64_t NumberNode::value() const
{
    return _value;
}

void NumberNode::accept(ASTVisitor& visitor) const
{
    visitor.visitNumber(*this);
}