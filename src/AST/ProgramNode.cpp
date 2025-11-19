#include "ProgramNode.hpp"

#include "ASTVisitor.hpp"

ProgramNode::ProgramNode(StmtList statements, size_t scopeId)
    : _statements(std::move(statements)), _scopeId(scopeId)
{
}

const ProgramNode::StmtList& ProgramNode::statements() const
{
    return _statements;
}

size_t ProgramNode::scopeId() const
{
    return _scopeId;
}

void ProgramNode::accept(ASTVisitor& visitor) const
{
    visitor.visitProgram(*this);
}