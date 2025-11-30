#pragma once

#include "ExprNode.hpp"
#include "ASTVisitor.hpp"

class BinaryOpNode : public ExprNode
{
public:
	enum class Operator
	{
		Add,
		Sub,
		Mul,
		Div,
		Equal,
		NotEqual
	};

	BinaryOpNode(Operator op, std::unique_ptr<ExprNode> left, std::unique_ptr<ExprNode> right);

	Operator op() const;
	const ExprNode* left() const;
	const ExprNode* right() const;

	void accept(ASTVisitor& visitor) const override;

private:
	Operator _op;
	std::unique_ptr<ExprNode> _left;
	std::unique_ptr<ExprNode> _right;
};