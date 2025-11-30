#pragma once

#include "ASTVisitor.hpp"
#include "BlockNode.hpp"
#include "ExprNode.hpp"
#include "StmtNode.hpp"
#include "TypeDesc.hpp"

class FunctionNode : public StmtNode
{
public:
	struct Param
	{
		TypeDesc type;
		std::string name;
		mutable SymbolID symbolId = InvalidSymbolID;
	};

	FunctionNode(std::string name, std::vector<Param> params, TypeDesc returnType, std::unique_ptr<BlockNode> body, size_t scopeId, std::string masterStruct = {});

	const std::string& name() const;
	const std::vector<Param>& params() const;
	const TypeDesc& returnType() const;
	const BlockNode* body() const;
	size_t scopeId() const;
	bool isMember() const;
	const std::string& masterStruct() const;

	void accept(ASTVisitor& visitor) const override;

private:
	std::string _name;
	std::vector<Param> _params;
	TypeDesc _returnType;
	std::unique_ptr<BlockNode> _body;
	size_t _scopeId;
	std::string _masterStruct;
};
