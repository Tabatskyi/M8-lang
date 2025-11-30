#pragma once

#include "ASTVisitor.hpp"
#include "ExprNode.hpp"
#include "StmtNode.hpp"
#include "TypeDesc.hpp"

class AssignNode : public StmtNode
{
public:
	AssignNode(std::string identifier, std::unique_ptr<ExprNode> value);

	const std::string& identifier() const;
	const ExprNode* value() const;
	ExprNode* value();
	void setValue(std::unique_ptr<ExprNode> value);
	SymbolID symbolId() const;
	void setSymbolId(SymbolID id) const;

	void accept(ASTVisitor& visitor) const override;

private:
	std::string _identifier;
	std::unique_ptr<ExprNode> _value;
	mutable SymbolID _symbolId = InvalidSymbolID;
};