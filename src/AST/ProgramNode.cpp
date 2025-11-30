#include "ProgramNode.hpp"

ProgramNode::ProgramNode(StmtList statements, size_t scopeId)
    : _statements(std::move(statements)), _scopeId(scopeId) {}

const ProgramNode::StmtList& ProgramNode::statements() const
{
    return _statements;
}

size_t ProgramNode::scopeId() const
{
    return _scopeId;
}

StmtNode* ProgramNode::appendStatement(std::unique_ptr<StmtNode> stmt)
{
    if (!stmt)
        return nullptr;
    StmtNode* raw = stmt.get();
    _statements.push_back(std::move(stmt));
    return raw;
}

void ProgramNode::accept(ASTVisitor& visitor) const
{
    visitor.visitProgram(*this);
}