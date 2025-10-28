#pragma once

#include "ASTNode.hpp"
#include "Semantics.hpp"

#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <limits>

using std::string;

struct IRContext
{
	int tempId =0;
	std::ostringstream ir;
};

struct CodegenValue
{
	string operand;
	ValueType type = ValueType::Invalid;
};

struct CodegenVariable
{
	ValueType type = ValueType::Invalid;
	bool isMutable = false;
	bool allocated = false;
	bool initialized = false;
	string pointer;
};

class CodeGenerator : public ASTVisitor
{
public:
	CodeGenerator(IRContext& ctx, const std::unordered_map<SymbolID, VariableInfo>& symbols);

	void generate(const ProgramNode& program);

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
	struct CodegenValue
	{
	std::string operand;
	ValueType type = ValueType::Invalid;
	};

	struct CodegenVariable
	{
	ValueType type = ValueType::Invalid;
	bool isMutable = false;
	bool allocated = false;
	bool initialized = false;
	std::string pointer;
	};

	std::string llvmType(ValueType type) const;
	std::string zeroLiteral(ValueType type) const;
	std::string nextTemp();
	std::string nextLabel(const std::string& base);
	void emitLabel(const std::string& label);
	void emitInstruction(const std::string& text);

	void pushValue(CodegenValue value);
	CodegenValue popValue();

	CodegenVariable& getVariable(SymbolID id);
	void ensureAllocated(CodegenVariable& var);

	CodegenValue ensureType(CodegenValue value, ValueType target);
	void storeValue(CodegenVariable& var, const CodegenValue& value);
	void emitReturn(CodegenValue value);

	bool generateBlock(const BlockNode& node, const std::string& exitLabel);

	IRContext& m_ctx;
	std::unordered_map<SymbolID, CodegenVariable> m_variables;
	std::vector<CodegenValue> m_stack;
	int m_labelId =0;
	bool m_currentBlockTerminated = false;
};