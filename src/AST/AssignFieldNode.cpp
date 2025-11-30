#include "AssignFieldNode.hpp"

AssignFieldNode::AssignFieldNode(std::unique_ptr<FieldAccessNode> target, std::unique_ptr<ExprNode> value)
    : _target(std::move(target)), _value(std::move(value))
{
}

const FieldAccessNode* AssignFieldNode::target() const
{
    return _target.get();
}

const ExprNode* AssignFieldNode::value() const
{
    return _value.get();
}

void AssignFieldNode::accept(ASTVisitor& visitor) const
{
    visitor.visitAssignField(*this);
}