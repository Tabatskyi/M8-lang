#include "AssignFieldNode.hpp"

AssignFieldNode::AssignFieldNode(std::unique_ptr<FieldAccessNode> target, std::unique_ptr<ExprNode> value)
    : _target(std::move(target)), _value(std::move(value))
{
}

const FieldAccessNode* AssignFieldNode::target() const
{
    return _target.get();
}

FieldAccessNode* AssignFieldNode::target()
{
    return _target.get();
}

void AssignFieldNode::setTarget(std::unique_ptr<FieldAccessNode> target)
{
    _target = std::move(target);
}

const ExprNode* AssignFieldNode::value() const
{
    return _value.get();
}

ExprNode* AssignFieldNode::value()
{
    return _value.get();
}

void AssignFieldNode::setValue(std::unique_ptr<ExprNode> value)
{
    _value = std::move(value);
}

void AssignFieldNode::accept(ASTVisitor& visitor) const
{
    visitor.visitAssignField(*this);
}