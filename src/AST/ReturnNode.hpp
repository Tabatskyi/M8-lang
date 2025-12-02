#pragma once

#include "ASTVisitor.hpp"
#include "ExprNode.hpp"
#include "StmtNode.hpp"

class ReturnNode : public StmtNode
{
public:
	explicit ReturnNode(std::unique_ptr<ExprNode> expr);

	const ExprNode* expr() const;
	ExprNode* expr();
	void setExpr(std::unique_ptr<ExprNode> expr);

	void accept(ASTVisitor& visitor) const override;

private:
	std::unique_ptr<ExprNode> _expr;
};