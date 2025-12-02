#pragma once

#include "ExprNode.hpp"
#include "FieldAccessNode.hpp"
#include "StmtNode.hpp"
#include "ASTVisitor.hpp"

class AssignFieldNode : public StmtNode
{
public:
	AssignFieldNode(std::unique_ptr<FieldAccessNode> target, std::unique_ptr<ExprNode> value);

	const FieldAccessNode* target() const;
	const ExprNode* value() const;
	FieldAccessNode* target();
	void setTarget(std::unique_ptr<FieldAccessNode> target);
	ExprNode* value();
	void setValue(std::unique_ptr<ExprNode> value);

	void accept(ASTVisitor& visitor) const override;

private:
	std::unique_ptr<FieldAccessNode> _target;
	std::unique_ptr<ExprNode> _value;
};