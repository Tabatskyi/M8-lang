#pragma once

#include "FactorNode.hpp"

class BoolLiteralNode : public FactorNode
{
public:
	explicit BoolLiteralNode(bool value);

	bool value() const;

	void accept(ASTVisitor& visitor) const override;

private:
	bool _value = false;
};
