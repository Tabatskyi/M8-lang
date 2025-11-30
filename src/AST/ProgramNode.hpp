#pragma once

#include "ASTVisitor.hpp"
#include "StmtNode.hpp"

class ProgramNode : public ASTNode
{
public:
	using StmtList = std::vector<std::unique_ptr<StmtNode>>;

	explicit ProgramNode(StmtList statements, size_t scopeId = 0);

	const StmtList& statements() const;
	StmtList& statements();
	size_t scopeId() const;
    StmtNode* appendStatement(std::unique_ptr<StmtNode> stmt);

	void accept(ASTVisitor& visitor) const override;

private:
	StmtList _statements;
	size_t _scopeId;
};