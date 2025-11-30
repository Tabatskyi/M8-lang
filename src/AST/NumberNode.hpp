#pragma once

#include "ASTVisitor.hpp"
#include "FactorNode.hpp"

class NumberNode : public FactorNode
{
public:
	explicit NumberNode(std::int64_t value);

	std::int64_t value() const;

	void accept(ASTVisitor& visitor) const override;

private:
	std::int64_t _value;
};