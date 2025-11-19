#pragma once

#include "../AST/ASTVisitor.hpp" // added visitor interface
#include "../AST/ASTFwd.hpp"      // forward declarations for node types
#include "../General/Utility.hpp"

#include <string>
#include <unordered_map>
#include <vector>

class SemanticAnalyzer : public ASTVisitor
{
public:
    bool analyze(const ProgramNode& program);

    const std::vector<std::string>& errors() const { return _errors; }
    const std::vector<std::string>& warnings() const { return _warnings; }

    const std::unordered_map<SymbolID, VariableInfo>& symbols() const { return _symbols; }

    void visitProgram(const ProgramNode& node) override;
    void visitBlock(const BlockNode& node) override;
    void visitDecl(const DeclNode& node) override;
    void visitAssign(const AssignNode& node) override;
    void visitAssignField(const AssignFieldNode& node) override; // added
    void visitIf(const IfNode& node) override;
    void visitReturn(const ReturnNode& node) override;
    void visitBinaryOp(const BinaryOpNode& node) override;
    void visitUnaryOp(const UnaryOpNode& node) override;
    void visitID(const IDNode& node) override;
    void visitNumber(const NumberNode& node) override;
    void visitBoolLiteral(const BoolLiteralNode& node) override;
    void visitStructDecl(const StructDeclNode& node) override; // added
    void visitFunction(const FunctionNode& node) override; // added
    void visitFieldAccess(const FieldAccessNode& node) override; // added
    void visitFunctionCall(const FunctionCallNode& node) override; // added
    void visitMemberFunctionCall(const MemberFunctionCallNode& node) override; // added

private:
    void enterScope(size_t scopeId);
    void exitScope();
    size_t currentScopeId() const;

    SymbolID resolveSymbol(const std::string& name) const;

    void addError(const std::string& message);
    void addWarning(const std::string& message);

    std::unordered_map<SymbolID, VariableInfo> _symbols;
    std::unordered_map<size_t, std::unordered_map<std::string, SymbolID>> _scopeSymbols;
    std::vector<size_t> _scopeStack;
    SymbolID _nextSymbolId = 0;
    std::vector<std::string> _errors;
    std::vector<std::string> _warnings;
    bool _returnSeen = false;
};