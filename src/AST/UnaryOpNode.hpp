#pragma once

#include <memory>

#include "ExprNode.hpp"

class UnaryOpNode : public ExprNode
{
public:
	enum class Operator
	{
		LogicalNot
	};

	UnaryOpNode(Operator op, std::unique_ptr<ExprNode> operand);

	Operator op() const;
	const ExprNode* operand() const;

	void accept(ASTVisitor& visitor) const override;

private:
	Operator _op;
	std::unique_ptr<ExprNode> _operand;
};