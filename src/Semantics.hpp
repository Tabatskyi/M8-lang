#pragma once

#include "ASTNode.hpp"
#include "Utility.hpp"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

class SemanticAnalyzer : public ASTVisitor
{
public:
    bool analyze(const ProgramNode& program);

    const std::vector<std::string>& errors() const { return m_errors; }
    const std::vector<std::string>& warnings() const { return m_warnings; }

    const std::unordered_map<SymbolID, VariableInfo>& symbols() const { return m_symbols; }

    void visitProgram(const ProgramNode& node) override;
    void visitBlock(const BlockNode& node) override;
    void visitDecl(const DeclNode& node) override;
    void visitAssign(const AssignNode& node) override;
    void visitIf(const IfNode& node) override;
    void visitReturn(const ReturnNode& node) override;
    void visitBinaryOp(const BinaryOpNode& node) override;
    void visitUnaryOp(const UnaryOpNode& node) override;
    void visitID(const IDNode& node) override;
    void visitNumber(const NumberNode& node) override;
    void visitBoolLiteral(const BoolLiteralNode& node) override;

private:
    void enterScope(size_t scopeId);
    void exitScope();
    size_t currentScopeId() const;

    SymbolID resolveSymbol(const std::string& name) const;

    void addError(const std::string& message);
    void addWarning(const std::string& message);

    std::unordered_map<SymbolID, VariableInfo> m_symbols;
    std::unordered_map<size_t, std::unordered_map<std::string, SymbolID>> m_scopeSymbols;
    std::vector<size_t> m_scopeStack;
    SymbolID m_nextSymbolId =0;
    std::vector<std::string> m_errors;
    std::vector<std::string> m_warnings;
    bool m_returnSeen = false;
};