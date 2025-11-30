#pragma once

#include "ASTVisitor.hpp"
#include "FunctionNode.hpp"
#include "StmtNode.hpp"
#include "TypeDesc.hpp"

class StructDeclNode : public StmtNode
{
public:
	struct Field
	{
		TypeDesc type;
		std::string name;
		bool isMutable = false;
	};

	StructDeclNode(std::string name,
				   std::vector<Field> fields,
				   std::vector<std::unique_ptr<class FunctionNode>> methods = {});

	const std::string& name() const;
	const std::vector<Field>& fields() const;
	const std::vector<std::unique_ptr<class FunctionNode>>& functions() const;

	void accept(ASTVisitor& visitor) const override;

private:
	std::string _name;
	std::vector<Field> _fields;
	std::vector<std::unique_ptr<class FunctionNode>> _functions;
};