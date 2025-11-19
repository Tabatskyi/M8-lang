#pragma once

#include <memory>
#include <vector>

#include "StmtNode.hpp"

class BlockNode : public ASTNode
{
public:
	using StmtList = std::vector<std::unique_ptr<StmtNode>>;

	BlockNode(StmtList statements, size_t scopeId);

	const StmtList& statements() const;
	size_t scopeId() const;

	void accept(ASTVisitor& visitor) const override;

private:
	StmtList _statements;
	size_t _scopeId;
};