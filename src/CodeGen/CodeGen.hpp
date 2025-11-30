#pragma once

#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <utility>

#include "../AST/ASTFwd.hpp"
#include "../AST/ASTVisitor.hpp"
#include "../AST/AssignFieldNode.hpp"
#include "../AST/AssignNode.hpp"
#include "../AST/BinaryOpNode.hpp"
#include "../AST/BlockNode.hpp"
#include "../AST/BoolLiteralNode.hpp"
#include "../AST/StringLiteralNode.hpp"
#include "../AST/ExprStmtNode.hpp"
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
#include "../AST/StructLiteralNode.hpp"
#include "../AST/UnaryOpNode.hpp"
#include "../Semantics/SemanticTypes.hpp"
#include "../General/Utility.hpp"

struct IRContext
{
    int tempId = 0;
    std::ostringstream ir;
};

struct CodegenValue
{
    std::string operand;
    bool isStruct = false;
    ValueType type = ValueType::Invalid;
    std::string structName;
};

struct CodegenVariable
{
    TypeDesc type{ TypeDesc::Builtin(ValueType::Invalid) };
    bool isMutable = false;
    bool allocated = false;
    bool initialized = false;
    std::string pointer;
    bool isGlobal = false;
};

class CodeGenerator : public ASTVisitor
{
public:
    struct StringLiteralInfo
    {
        std::string globalName;
        std::string encodedValue;
        size_t length = 0;
        bool emitted = false;
    };

    CodeGenerator(IRContext& ctx,
              const std::unordered_map<SymbolID, VariableInfo>& symbols,
              const StructTable& structs,
              const FunctionTable& functions,
              size_t globalScopeId);

    void emitTopLevel(const ProgramNode& program);
    void planGlobalInit(const ProgramNode& program);
    void emitGlobalInit();
    void generate(const ProgramNode& program);

    void visitProgram(const ProgramNode& node) override;
    void visitBlock(const BlockNode& node) override;
    void visitFunction(const FunctionNode& node) override;
    void visitDecl(const DeclNode& node) override;
    void visitStructDecl(const StructDeclNode& node) override;
    void visitAssign(const AssignNode& node) override;
    void visitAssignField(const AssignFieldNode& node) override;
    void visitExprStmt(const ExprStmtNode& node) override;
    void visitFieldAccess(const FieldAccessNode& node) override;
    void visitFunctionCall(const FunctionCallNode& node) override;
    void visitMemberFunctionCall(const MemberFunctionCallNode& node) override;
    void visitIf(const IfNode& node) override;
    void visitReturn(const ReturnNode& node) override;
    void visitBinaryOp(const BinaryOpNode& node) override;
    void visitUnaryOp(const UnaryOpNode& node) override;
    void visitID(const IDNode& node) override;
    void visitNumber(const NumberNode& node) override;
    void visitBoolLiteral(const BoolLiteralNode& node) override;
    void visitStringLiteral(const StringLiteralNode& node) override;
    void visitStructLiteral(const StructLiteralNode& node) override;

    void emitStringLiteralGlobals();
    bool hasGlobalInit() const { return _hasGlobalInit; }
    const std::string& globalInitName() const { return _globalInitName; }

private:
    void generateFunction(const FunctionNode& func);
    const FunctionInfo* findMemberFunction(const std::string& funcName, const std::string& structName) const;
    std::string llvmType(ValueType type) const;
    std::string zeroLiteral(ValueType type) const;
    std::string nextTemp();
    std::string nextLabel(const std::string& base);
    void emitLabel(const std::string& label);
    void emitInstruction(const std::string& text);
    void pushValue(CodegenValue value);
    CodegenValue popValue();
    TypeDesc fieldTypeDesc(const FieldAccessNode& node);
    std::string getFieldPointer(const FieldAccessNode& node);
    CodegenVariable& getVariable(SymbolID id);
    void ensureAllocated(CodegenVariable& var);
    CodegenValue ensureType(CodegenValue value, ValueType target);
    void storeValue(CodegenVariable& var, const CodegenValue& value);
    void emitReturn(CodegenValue value);
    bool generateBlock(const BlockNode& node, const std::string& exitLabel);
    bool handleBuiltinFunctionCall(const FunctionCallNode& node, const FunctionInfo& info);
    void emitStructDefinition(const StructDeclNode& node);
    void emitStructDefinition(const StructInfo& info);
    void emitGlobalDeclarations();
    const StringLiteralInfo& internStringLiteral(const std::string& literal);
    std::string formatPointer(const std::string& symbol, size_t length);

    IRContext& _ctx;
    std::unordered_map<SymbolID, CodegenVariable> _variables;
    std::vector<CodegenValue> _stack;
    int _labelId = 0;
    bool _currentBlockTerminated = false;
    const StructTable& _structs;
    const FunctionTable& _functions;
    bool _inFunction = false;
    TypeDesc _functionReturnType{ TypeDesc::Builtin(ValueType::Invalid) };
    bool _emittingTopLevel = false;
    std::unordered_set<std::string> _emittedStructs;
    std::string _currentMemberMaster;
    std::string _currentMemberFunctionName;
    SymbolID _selfSymbolId = InvalidSymbolID;
    std::unordered_map<std::string, StringLiteralInfo> _stringLiterals;
    int _stringLiteralCounter = 0;
    bool _hasGlobalInit = false;
    std::string _globalInitName = "__m8_global_init";
    size_t _globalScopeId = 0;
    bool _globalsDeclared = false;
    std::vector<const StmtNode*> _globalInitStmts;
};