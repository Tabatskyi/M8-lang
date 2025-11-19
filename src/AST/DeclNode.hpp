#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ExprNode.hpp"
#include "StmtNode.hpp"
#include "TypeDesc.hpp"

class DeclNode : public StmtNode
{
public:
	DeclNode(TypeDesc type, std::string identifier, bool isMutable, std::vector<std::unique_ptr<ExprNode>> initializers);

	const TypeDesc& declaredType() const;
	const std::string& identifier() const;
	bool isMutable() const;
	bool hasInitializer() const;
	const std::vector<std::unique_ptr<ExprNode>>& initializers() const;
	SymbolID symbolId() const;
	void setSymbolId(SymbolID id) const;

	void accept(ASTVisitor& visitor) const override;

private:
	TypeDesc _declType;
	std::string _identifier;
	bool _isMutable;
	std::vector<std::unique_ptr<ExprNode>> _initializers;
	mutable SymbolID _symbolId = InvalidSymbolID;
};
