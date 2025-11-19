#include "UnaryOpNode.hpp"

#include "ASTVisitor.hpp"

UnaryOpNode::UnaryOpNode(Operator op, std::unique_ptr<ExprNode> operand)
    : _op(op), _operand(std::move(operand))
{
}

UnaryOpNode::Operator UnaryOpNode::op() const
{
    return _op;
}

const ExprNode* UnaryOpNode::operand() const
{
    return _operand.get();
}

void UnaryOpNode::accept(ASTVisitor& visitor) const
{
    visitor.visitUnaryOp(*this);
}