#pragma once

#include "ASTVisitor.hpp"
#include "ExprNode.hpp"
#include "FactorNode.hpp"

class MemberFunctionCallNode : public FactorNode
{
public:
	MemberFunctionCallNode(std::string base, std::vector<std::string> chain, std::string function, std::vector<std::unique_ptr<ExprNode>> arguments);

	const std::string& base() const;
	const std::vector<std::string>& fieldChain() const;
	const std::string& funcName() const;
	const std::vector<std::unique_ptr<ExprNode>>& args() const;
	SymbolID baseSymbolId() const;
	void setBaseSymbolId(SymbolID id) const;

	void accept(ASTVisitor& visitor) const override;

private:
	std::string _base;
	std::vector<std::string> _fields;
	std::string _funcName;
	std::vector<std::unique_ptr<ExprNode>> _args;
	mutable SymbolID _baseId = InvalidSymbolID;
};