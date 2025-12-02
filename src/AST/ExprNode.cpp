#include "ExprNode.hpp"

ExprNode::~ExprNode() = default;

ValueType ExprNode::type() const
{
    return _type;
}

void ExprNode::setType(ValueType type) const
{
    _type = type;
}
