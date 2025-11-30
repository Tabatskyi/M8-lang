#pragma once

#include <memory>
#include <string>
#include <vector>
#include <limits>
#include <sstream>
#include <utility>
#include <unordered_map>

#include "../AST/ASTVisitor.hpp"
#include "../AST/AssignFieldNode.hpp"
#include "../AST/AssignNode.hpp"
#include "../AST/BinaryOpNode.hpp"
#include "../AST/BlockNode.hpp"
#include "../AST/BoolLiteralNode.hpp"
#include "../AST/DeclNode.hpp"
#include "../AST/FieldAccessNode.hpp"
#include "../AST/FunctionCallNode.hpp"
#include "../AST/FunctionNode.hpp"
#include "../AST/IDNode.hpp"
#include "../AST/IfNode.hpp"
#include "../AST/MemberFunctionCallNode.hpp"
#include "../AST/NumberNode.hpp"
#include "../AST/ProgramNode.hpp"
#include "../AST/ReturnNode.hpp"
#include "../AST/StructDecNode.hpp"
#include "../AST/UnaryOpNode.hpp"
#include "../General/Utility.hpp"
#include "SemanticTypes.hpp"

class SemanticAnalyzer : public ASTVisitor
{
public:
    bool analyze(const ProgramNode& program);

    const std::vector<std::string>& errors() const { return _errors; }
    const std::vector<std::string>& warnings() const { return _warnings; }

    const std::unordered_map<SymbolID, VariableInfo>& symbols() const { return _symbols; }
    const StructTable& structs() const { return _structs; }
    const FunctionTable& functions() const { return _functions; }

    void visitProgram(const ProgramNode& node) override;
    void visitBlock(const BlockNode& node) override;
    void visitFunction(const FunctionNode& node) override;
    void visitDecl(const DeclNode& node) override;
    void visitAssign(const AssignNode& node) override;
    void visitAssignField(const AssignFieldNode& node) override;
    void visitIf(const IfNode& node) override;
    void visitReturn(const ReturnNode& node) override;
    void visitBinaryOp(const BinaryOpNode& node) override;
    void visitUnaryOp(const UnaryOpNode& node) override;
    void visitID(const IDNode& node) override;
    void visitNumber(const NumberNode& node) override;
    void visitBoolLiteral(const BoolLiteralNode& node) override;
    void visitFieldAccess(const FieldAccessNode& node) override;
    void visitMemberFunctionCall(const MemberFunctionCallNode& node) override;
    void visitFunctionCall(const FunctionCallNode& node) override;
    void visitStructDecl(const StructDeclNode& node) override;

private:
    void enterScope(size_t scopeId);
    void exitScope();
    size_t currentScopeId() const;
    SymbolID resolveSymbol(const std::string& name) const;

    void addError(const std::string& message);
    void addError(const std::string& message, const ASTNode& node);
    void addError(const std::string& message, const ASTNode* node);
    void addWarning(const std::string& message);
    void addWarning(const std::string& message, const ASTNode& node);
    void addWarning(const std::string& message, const ASTNode* node);

    void validateCallArguments(const std::vector<std::unique_ptr<ExprNode>>& args, const FunctionInfo& funcInfo, size_t paramStartIndex, const std::string& undeclaredVarMessage);

    const FunctionInfo* findMemberFunction(const std::string& funcName, const std::string& structName) const;
    TypeDesc resolveFieldType(SymbolID baseId, const std::vector<std::string>& fieldChain, const ASTNode* reporter);
    bool ensureFieldChainMutable(const VariableInfo& baseVar, const std::vector<std::string>& fieldChain, const ASTNode* reporter);
    void registerBuiltins();

    std::vector<std::string> _errors;
    std::vector<std::string> _warnings;
    std::unordered_map<SymbolID, VariableInfo> _symbols;
    std::unordered_map<size_t, std::unordered_map<std::string, SymbolID>> _scopeSymbols;
    std::vector<size_t> _scopeStack;
    SymbolID _nextSymbolId = 0;
    StructTable _structs;
    FunctionTable _functions;
    bool _inFunction = false;
    bool _returnSeen = false;
    TypeDesc _currentFunctionReturn{ TypeDesc::Builtin(ValueType::Invalid) };
    std::string _currentMemberMaster;
};