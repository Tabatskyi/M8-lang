#pragma once

#include "ASTNode.hpp"
#include "Token.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

class SyntaxParser
{
public:
    explicit SyntaxParser(std::vector<Token> tokens);

    const Token* peek(size_t offset = 0) const;

    const Token* eat();

    std::unique_ptr<ProgramNode> parseProgram();

    std::unique_ptr<StmtNode> parseStmt();

    std::unique_ptr<ReturnNode> parseReturn();
    std::unique_ptr<IfNode> parseIf();
    std::unique_ptr<BlockNode> parseBlock(std::unique_ptr<StmtNode> stmt, size_t scopeId);

    std::unique_ptr<DeclNode> parseDecl();
    std::unique_ptr<AssignNode> parseAssign();

    std::unique_ptr<ExprNode> parseExpr();
    std::unique_ptr<ExprNode> parseEquality();
    std::unique_ptr<ExprNode> parseAdditive();
    std::unique_ptr<ExprNode> parseMultiplicative();
    std::unique_ptr<ExprNode> parseUnary();
    std::unique_ptr<ExprNode> parsePrimary();

    bool hasErrors() const { return !m_errors.empty(); }

    const std::vector<std::string>& errors() const { return m_errors; }

private:
    bool atEnd() const;
    bool match(TokenType type);
    bool expect(TokenType type, const std::string& message);

    void skipNewlines();
    ValueType parseType();
    size_t allocateScopeId();

    void addError(const std::string& message);

    std::vector<Token> m_tokens;
    size_t m_index = 0;
    std::vector<std::string> m_errors;
    size_t m_nextScopeId = 1;
};
