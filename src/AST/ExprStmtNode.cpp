#include "ExprStmtNode.hpp"

#include "ASTVisitor.hpp"

ExprStmtNode::ExprStmtNode(std::unique_ptr<ExprNode> expr)
    : _expr(std::move(expr))
{
}

const ExprNode* ExprStmtNode::expr() const
{
    return _expr.get();
}

void ExprStmtNode::accept(ASTVisitor& visitor) const
{
    visitor.visitExprStmt(*this);
}
