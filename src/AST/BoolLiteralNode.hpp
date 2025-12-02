#pragma once

#include "FactorNode.hpp"
#include "ASTVisitor.hpp"

class BoolLiteralNode : public FactorNode
{
public:
	explicit BoolLiteralNode(bool value);

	bool value() const;

	void accept(ASTVisitor& visitor) const override;

private:
	bool _value = false;
};
