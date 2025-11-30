#pragma once

#include <memory>

#include "ExprNode.hpp"
#include "StmtNode.hpp"

class ExprStmtNode : public StmtNode
{
public:
    explicit ExprStmtNode(std::unique_ptr<ExprNode> expr);

    const ExprNode* expr() const;

    void accept(ASTVisitor& visitor) const override;

private:
    std::unique_ptr<ExprNode> _expr;
};
