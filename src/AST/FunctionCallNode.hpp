#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ExprNode.hpp"
#include "FactorNode.hpp"

class FunctionCallNode : public FactorNode
{
public:
	FunctionCallNode(std::string name, std::vector<std::unique_ptr<ExprNode>> arguments);

	const std::string& name() const;
	const std::vector<std::unique_ptr<ExprNode>>& args() const;

	void setSymbolId(SymbolID id) const;
	SymbolID symbolId() const;

	void accept(ASTVisitor& visitor) const override;

private:
	std::string _name;
	std::vector<std::unique_ptr<ExprNode>> _arguments;
	mutable SymbolID _symbolId = InvalidSymbolID;
};
