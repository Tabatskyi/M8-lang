#include "BinaryOpNode.hpp"

BinaryOpNode::BinaryOpNode(Operator op, std::unique_ptr<ExprNode> left, std::unique_ptr<ExprNode> right)
    : _op(op), _left(std::move(left)), _right(std::move(right)) {}

BinaryOpNode::Operator BinaryOpNode::op() const
{
    return _op;
}

const ExprNode* BinaryOpNode::left() const
{
    return _left.get();
}

const ExprNode* BinaryOpNode::right() const
{
    return _right.get();
}

void BinaryOpNode::accept(ASTVisitor& visitor) const
{
    visitor.visitBinaryOp(*this);
}