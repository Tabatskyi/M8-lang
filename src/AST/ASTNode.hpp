#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Utility.hpp"

class ProgramNode;
class BlockNode;
class StmtNode;
class ExprNode;
class FactorNode;
class ReturnNode;
class DeclNode;
class AssignNode;
class IfNode;
class IDNode;
class NumberNode;
class BoolLiteralNode;
class BinaryOpNode;
class UnaryOpNode;

class ASTVisitor
{
public:
	virtual ~ASTVisitor() = default;

	virtual void visitProgram(const ProgramNode& node) = 0;
	virtual void visitBlock(const BlockNode& node) = 0;
	virtual void visitDecl(const DeclNode& node) = 0;
	virtual void visitAssign(const AssignNode& node) = 0;
	virtual void visitIf(const IfNode& node) = 0;
	virtual void visitReturn(const ReturnNode& node) = 0;
	virtual void visitBinaryOp(const BinaryOpNode& node) = 0;
	virtual void visitUnaryOp(const UnaryOpNode& node) = 0;
	virtual void visitID(const IDNode& node) = 0;
	virtual void visitNumber(const NumberNode& node) = 0;
	virtual void visitBoolLiteral(const BoolLiteralNode& node) = 0;
};

class ASTNode
{
public:
	virtual ~ASTNode() = default;
	virtual void accept(ASTVisitor& visitor) const = 0;
};

class StmtNode : public ASTNode
{
public:
	~StmtNode() override = default;
};

class ExprNode : public ASTNode
{
public:
	~ExprNode() override = default;

	ValueType type() const { return _type; }
	void setType(ValueType type) const { _type = type; }

private:
	mutable ValueType _type = ValueType::Invalid;
};

class FactorNode : public ExprNode
{
public:
	~FactorNode() override = default;
};

class ProgramNode : public ASTNode
{
public:
	using StmtList = std::vector<std::unique_ptr<StmtNode>>;

	explicit ProgramNode(StmtList stmts, size_t scopeId = 0)
		: _statements(std::move(stmts)), _scopeId(scopeId) {}

	const StmtList& statements() const { return _statements; }
	size_t scopeId() const { return _scopeId; }

	void accept(ASTVisitor& visitor) const override { visitor.visitProgram(*this); }

private:
	StmtList _statements;
	size_t _scopeId;
};

class BlockNode : public ASTNode
{
public:
	using StmtList = std::vector<std::unique_ptr<StmtNode>>;

	BlockNode(StmtList stmts, size_t scopeId)
		: _statements(std::move(stmts)), _scopeId(scopeId) {}

	const StmtList& statements() const { return _statements; }
	size_t scopeId() const { return _scopeId; }

	void accept(ASTVisitor& visitor) const override { visitor.visitBlock(*this); }

private:
	StmtList _statements;
	size_t _scopeId;
};

class IDNode : public FactorNode
{
public:
	explicit IDNode(std::string name) : _name(std::move(name)) {}

	const std::string& name() const { return _name; }
	SymbolID symbolId() const { return _symbolId; }
	void setSymbolId(SymbolID id) const { _symbolId = id; }

	void accept(ASTVisitor& visitor) const override { visitor.visitID(*this); }

private:
	std::string _name;
	mutable SymbolID _symbolId = InvalidSymbolID;
};

class NumberNode : public FactorNode
{
public:
	explicit NumberNode(std::int64_t value) : _value(value) {}

	std::int64_t value() const { return _value; }

	void accept(ASTVisitor& visitor) const override { visitor.visitNumber(*this); }

private:
	std::int64_t _value;
};

class BoolLiteralNode : public FactorNode
{
public:
	explicit BoolLiteralNode(bool value) : _value(value) {}

	bool value() const { return _value; }

	void accept(ASTVisitor& visitor) const override { visitor.visitBoolLiteral(*this); }

private:
	bool _value;
};

class BinaryOpNode : public ExprNode
{
public:
	enum class Operator
	{
		Add,
		Sub,
		Mul,
		Div,
		Equal,
		NotEqual
	};

	BinaryOpNode(Operator op, std::unique_ptr<ExprNode> left, std::unique_ptr<ExprNode> right)
				: _operator(op), _left(std::move(left)), _right(std::move(right)) {}

	Operator op() const { return _operator; }
	const ExprNode* left() const { return _left.get(); }
	const ExprNode* right() const { return _right.get(); }

	void accept(ASTVisitor& visitor) const override { visitor.visitBinaryOp(*this); }

private:
	Operator _operator;
	std::unique_ptr<ExprNode> _left;
	std::unique_ptr<ExprNode> _right;
};

class UnaryOpNode : public ExprNode
{
public:
	enum class Operator
	{
		LogicalNot
	};

	UnaryOpNode(Operator op, std::unique_ptr<ExprNode> operand)
		: _operator(op), _operand(std::move(operand)) {}

	Operator op() const { return _operator; }
	const ExprNode* operand() const { return _operand.get(); }

	void accept(ASTVisitor& visitor) const override { visitor.visitUnaryOp(*this); }

private:
	Operator _operator;
	std::unique_ptr<ExprNode> _operand;
};

class DeclNode : public StmtNode
{
public:
	DeclNode(ValueType type, std::string identifier, bool isMutable, std::unique_ptr<ExprNode> initializer)
			: _type(type), _identifier(std::move(identifier)), _isMutable(isMutable), _initializer(std::move(initializer)) {}

	ValueType declaredType() const { return _type; }
	const std::string& identifier() const { return _identifier; }
	bool isMutable() const { return _isMutable; }
	bool hasInitializer() const { return static_cast<bool>(_initializer); }
	const ExprNode* initializer() const { return _initializer.get(); }
	SymbolID symbolId() const { return _symbolId; }
	void setSymbolId(SymbolID id) const { _symbolId = id; }

	void accept(ASTVisitor& visitor) const override { visitor.visitDecl(*this); }

private:
	ValueType _type;
	std::string _identifier;
	bool _isMutable;
	std::unique_ptr<ExprNode> _initializer;
	mutable SymbolID _symbolId = InvalidSymbolID;
};

class AssignNode : public StmtNode
{
public:
	AssignNode(std::string identifier, std::unique_ptr<ExprNode> value)
		: _identifier(std::move(identifier)), _value(std::move(value)) {}

	const std::string& identifier() const { return _identifier; }
	const ExprNode* value() const { return _value.get(); }
	SymbolID symbolId() const { return _symbolId; }
	void setSymbolId(SymbolID id) const { _symbolId = id; }

	void accept(ASTVisitor& visitor) const override { visitor.visitAssign(*this); }

private:
	std::string _identifier;
	std::unique_ptr<ExprNode> _value;
	mutable SymbolID _symbolId = InvalidSymbolID;
};

class IfNode : public StmtNode
{
public:
	IfNode(std::unique_ptr<ExprNode> condition, std::unique_ptr<BlockNode> thenBlock, std::unique_ptr<BlockNode> elseBlock)
			: _condition(std::move(condition)), _then(std::move(thenBlock)), _else(std::move(elseBlock)) {}

	const ExprNode* condition() const { return _condition.get(); }
	const BlockNode* thenBlock() const { return _then.get(); }
	const BlockNode* elseBlock() const { return _else.get(); }

	void accept(ASTVisitor& visitor) const override { visitor.visitIf(*this); }

private:
	std::unique_ptr<ExprNode> _condition;
	std::unique_ptr<BlockNode> _then;
	std::unique_ptr<BlockNode> _else;
};

class ReturnNode : public StmtNode
{
public:
	explicit ReturnNode(std::unique_ptr<ExprNode> expr)
		: _expr(std::move(expr)) {}

	const ExprNode* expr() const { return _expr.get(); }

	void accept(ASTVisitor& visitor) const override { visitor.visitReturn(*this); }

private:
	std::unique_ptr<ExprNode> _expr;
};