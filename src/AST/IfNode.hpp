#pragma once

#include <memory>

#include "BlockNode.hpp"
#include "ExprNode.hpp"
#include "StmtNode.hpp"

class IfNode : public StmtNode
{
public:
	IfNode(std::unique_ptr<ExprNode> condition, std::unique_ptr<BlockNode> thenBlock, std::unique_ptr<BlockNode> elseBlock);

	const ExprNode* condition() const;
	const BlockNode* thenBlock() const;
	const BlockNode* elseBlock() const;

	void accept(ASTVisitor& visitor) const override;

private:
	std::unique_ptr<ExprNode> _condition;
	std::unique_ptr<BlockNode> _thenBlock;
	std::unique_ptr<BlockNode> _elseBlock;
};
