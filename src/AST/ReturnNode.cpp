#include "ReturnNode.hpp"

ReturnNode::ReturnNode(std::unique_ptr<ExprNode> expr)
    : _expr(std::move(expr)) {}

const ExprNode* ReturnNode::expr() const
{
    return _expr.get();
}

ExprNode* ReturnNode::expr()
{
    return _expr.get();
}

void ReturnNode::setExpr(std::unique_ptr<ExprNode> expr)
{
    _expr = std::move(expr);
}

void ReturnNode::accept(ASTVisitor& visitor) const
{
    visitor.visitReturn(*this);
}