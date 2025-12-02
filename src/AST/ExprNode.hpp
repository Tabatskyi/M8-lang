#pragma once

#include "ASTNode.hpp"
#include "TypeDesc.hpp"

class ExprNode : public ASTNode
{
public:
	~ExprNode() override;

	ValueType type() const;
	void setType(ValueType type) const;

private:
	mutable ValueType _type = ValueType::Invalid;
};
