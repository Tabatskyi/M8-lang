#pragma once

#include "ASTVisitor.hpp"
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
	ExprNode* condition();
	void setCondition(std::unique_ptr<ExprNode> condition);
	BlockNode* thenBlock();
	BlockNode* elseBlock();
	void setThenBlock(std::unique_ptr<BlockNode> thenBlock);
	void setElseBlock(std::unique_ptr<BlockNode> elseBlock);

	void accept(ASTVisitor& visitor) const override;

private:
	std::unique_ptr<ExprNode> _condition;
	std::unique_ptr<BlockNode> _thenBlock;
	std::unique_ptr<BlockNode> _elseBlock;
};
