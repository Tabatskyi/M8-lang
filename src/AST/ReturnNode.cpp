#include "ReturnNode.hpp"

ReturnNode::ReturnNode(std::unique_ptr<ExprNode> expr)
    : _expr(std::move(expr)) {}

const ExprNode* ReturnNode::expr() const
{
    return _expr.get();
}

void ReturnNode::accept(ASTVisitor& visitor) const
{
    visitor.visitReturn(*this);
}