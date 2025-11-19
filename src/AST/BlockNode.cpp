#include "BlockNode.hpp"

#include "ASTVisitor.hpp"

BlockNode::BlockNode(StmtList statements, size_t scopeId)
    : _statements(std::move(statements)), _scopeId(scopeId) {}

const BlockNode::StmtList& BlockNode::statements() const
{
    return _statements;
}

size_t BlockNode::scopeId() const
{
    return _scopeId;
}

void BlockNode::accept(ASTVisitor& visitor) const
{
    visitor.visitBlock(*this);
}