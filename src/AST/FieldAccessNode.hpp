#pragma once

#include <string>
#include <vector>

#include "FactorNode.hpp"
#include "TypeDesc.hpp"

class FieldAccessNode : public FactorNode
{
public:
	FieldAccessNode(std::string base, std::vector<std::string> chain);

	const std::string& base() const;
	const std::vector<std::string>& fieldChain() const;
	SymbolID baseSymbolId() const;
	void setBaseSymbolId(SymbolID id) const;

	void accept(ASTVisitor& visitor) const override;

private:
	std::string _base;
	std::vector<std::string> _fields;
	mutable SymbolID _baseId = InvalidSymbolID;
};
