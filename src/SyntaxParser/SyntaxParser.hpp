#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../AST/ASTFwd.hpp"
#include "../AST/TypeDesc.hpp"
#include "../AST/ProgramNode.hpp"
#include "../AST/StmtNode.hpp"
#include "../AST/BlockNode.hpp"
#include "../AST/ReturnNode.hpp"
#include "../AST/DeclNode.hpp"
#include "../AST/AssignNode.hpp"
#include "../AST/AssignFieldNode.hpp"
#include "../AST/IfNode.hpp"
#include "../AST/BinaryOpNode.hpp"
#include "../AST/UnaryOpNode.hpp"
#include "../AST/IDNode.hpp"
#include "../AST/NumberNode.hpp"
#include "../AST/BoolLiteralNode.hpp"
#include "../AST/FunctionNode.hpp"
#include "../AST/StructDecNode.hpp"
#include "../AST/FunctionCallNode.hpp"
#include "../AST/MemberFunctionCallNode.hpp"
#include "../AST/FieldAccessNode.hpp"
#include "../General/Token.hpp"

class SyntaxParser
{
public:
    explicit SyntaxParser(std::vector<Token> tokens);

    const Token* peek(size_t offset = 0) const;
    const Token* eat();

    std::unique_ptr<ProgramNode> parseProgram();
    std::unique_ptr<StmtNode> parseStmt();

    std::unique_ptr<FunctionNode> parseFunction(const std::string& masterStruct = {});
    std::unique_ptr<StructDeclNode> parseStruct();

    std::unique_ptr<ReturnNode> parseReturn();
    std::unique_ptr<IfNode> parseIf();
    std::unique_ptr<BlockNode> parseBlock(std::vector<std::unique_ptr<StmtNode>> statements, size_t scopeId);

    std::unique_ptr<DeclNode> parseDecl();
    std::unique_ptr<StmtNode> parseAssign();

    std::unique_ptr<ExprNode> parseExpr();
    std::unique_ptr<ExprNode> parseEquality();
    std::unique_ptr<ExprNode> parseAdditive();
    std::unique_ptr<ExprNode> parseMultiplicative();
    std::unique_ptr<ExprNode> parseUnary();
    std::unique_ptr<ExprNode> parsePrimary();

    bool hasErrors() const { return !_errors.empty(); }
    const std::vector<std::string>& errors() const { return _errors; }

private:
    bool atEnd() const;
    bool match(TokenType type);
    bool expect(TokenType type, const std::string& message);

    void skipNewlines();
    TypeDesc parseTypeDesc();
    size_t allocateScopeId();

    void addError(const std::string& message);

    std::vector<Token> _tokens;
    size_t _index = 0;
    std::vector<std::string> _errors;
    size_t _nextScopeId = 1;
};