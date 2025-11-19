#pragma once

#include "ASTFwd.hpp"

class ASTVisitor
{
public:
	virtual ~ASTVisitor() = default;

	virtual void visitProgram(const ProgramNode& node) = 0;
	virtual void visitBlock(const BlockNode& node) = 0;
	virtual void visitDecl(const DeclNode& node) = 0;
	virtual void visitAssign(const AssignNode& node) = 0;
	virtual void visitAssignField(const AssignFieldNode& node) = 0;
	virtual void visitIf(const IfNode& node) = 0;
	virtual void visitReturn(const ReturnNode& node) = 0;
	virtual void visitBinaryOp(const BinaryOpNode& node) = 0;
	virtual void visitUnaryOp(const UnaryOpNode& node) = 0;
	virtual void visitID(const IDNode& node) = 0;
	virtual void visitNumber(const NumberNode& node) = 0;
	virtual void visitBoolLiteral(const BoolLiteralNode& node) = 0;
	virtual void visitStructDecl(const StructDeclNode& node) = 0;
	virtual void visitFunction(const FunctionNode& node) = 0;
	virtual void visitFieldAccess(const FieldAccessNode& node) = 0;
	virtual void visitFunctionCall(const FunctionCallNode& node) = 0;
	virtual void visitMemberFunctionCall(const MemberFunctionCallNode& node) = 0;
};