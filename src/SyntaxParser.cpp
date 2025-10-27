#include "SyntaxParser.hpp"

SyntaxParser::SyntaxParser(std::vector<Token> tokens): m_tokens(std::move(tokens)) {}

const Token* SyntaxParser::peek(size_t offset) const
{
    size_t target = m_index + offset;
    if (target >= m_tokens.size())
        return nullptr;
    return& m_tokens[target];
}

const Token* SyntaxParser::eat()
{
    if (atEnd())
        return nullptr;
    return& m_tokens[m_index++];
}

std::unique_ptr<ProgramNode> SyntaxParser::parseProgram()
{
    ProgramNode::StmtList statements;

    skipNewlines();

    while (!atEnd())
    {
        auto stmt = parseStmt();
        if (!stmt)
            return nullptr;

        statements.push_back(std::move(stmt));
        skipNewlines();
    }

    if (!atEnd())
        addError("Unexpected tokens after program body");

    return std::make_unique<ProgramNode>(std::move(statements), 0);
}

std::unique_ptr<StmtNode> SyntaxParser::parseStmt()
{
    skipNewlines();

    const Token* token = peek();
    if (!token)
    {
        addError("Unexpected end of input while parsing statement");
        return nullptr;
    }

    switch (token->type)
    {
        case TokenType::I32:
        case TokenType::I64:
        case TokenType::Bool:
            return parseDecl();

        case TokenType::Identifier:
            return parseAssign();

        case TokenType::Return:
            return parseReturn();

        case TokenType::If:
            return parseIf();

        default:
            addError("Unexpected token '" + token->lexeme + "' at start of statement");
            return nullptr;
    }
}

std::unique_ptr<ReturnNode> SyntaxParser::parseReturn()
{
    if (!expect(TokenType::Return, "Expected 'return'"))
        return nullptr;

    skipNewlines();
    auto expr = parseExpr();
    if (!expr)
        return nullptr;

    return std::make_unique<ReturnNode>(std::move(expr));
}

std::unique_ptr<IfNode> SyntaxParser::parseIf()
{
    if (!expect(TokenType::If, "Expected 'if'"))
        return nullptr;

    skipNewlines();
    auto condition = parseExpr();
    if (!condition)
        return nullptr;

    skipNewlines();
    auto thenBlock = parseBlock(allocateScopeId());
    if (!thenBlock)
        return nullptr;

    skipNewlines();
    std::unique_ptr<BlockNode> elseBlock;
    if (match(TokenType::Else))
    {
        skipNewlines();
        elseBlock = parseBlock(allocateScopeId());
        if (!elseBlock)
            return nullptr;
        skipNewlines();
    }

    return std::make_unique<IfNode>(std::move(condition), std::move(thenBlock), std::move(elseBlock));
}

std::unique_ptr<BlockNode> SyntaxParser::parseBlock(size_t scopeId)
{
    if (!expect(TokenType::BlockStart, "Expected '{' to start block"))
        return nullptr;

    BlockNode::StmtList statements;
    skipNewlines();

    while (true)
    {
        const Token* token = peek();
        if (!token)
        {
            addError("Unexpected end of input inside block");
            return nullptr;
        }

        if (token->type == TokenType::BlockEnd)
        {
            eat();
            break;
        }

        auto stmt = parseStmt();
        if (!stmt)
            return nullptr;

        statements.push_back(std::move(stmt));
        skipNewlines();
    }

    return std::make_unique<BlockNode>(std::move(statements), scopeId);
}

std::unique_ptr<DeclNode> SyntaxParser::parseDecl()
{
    ValueType type = parseType();
    if (type == ValueType::Invalid)
        return nullptr;

    skipNewlines();

    bool isMutable = false;
    while (match(TokenType::Mut))
    {
        isMutable = true;
        skipNewlines();
    }

    const Token* identTok = peek();
    if (!identTok || identTok->type != TokenType::Identifier)
    {
        addError("Expected identifier after type specifier");
        return nullptr;
    }
    std::string identifier = identTok->lexeme;
    eat();

    skipNewlines();

    std::unique_ptr<ExprNode> initializer;
    if (match(TokenType::BlockStart))
    {
        skipNewlines();
        initializer = parseExpr();
        if (!initializer)
            return nullptr;

        skipNewlines();
        if (!expect(TokenType::BlockEnd, "Expected '}' after initializer expression"))
            return nullptr;
    }

    return std::make_unique<DeclNode>(type, std::move(identifier), isMutable, std::move(initializer));
}

ValueType SyntaxParser::parseType()
{
    const Token* token = peek();
    if (!token)
    {
        addError("Expected type specifier");
        return ValueType::Invalid;
    }

    switch (token->type)
    {
        case TokenType::I32:
            eat();
            return ValueType::I32;

        case TokenType::I64:
            eat();
            return ValueType::I64;

        case TokenType::Bool:
            eat();
            return ValueType::Bool;

        default:
            addError("Expected type specifier");
            return ValueType::Invalid;
    }
}

std::unique_ptr<AssignNode> SyntaxParser::parseAssign()
{
    const Token* identTok = peek();
    if (!identTok || identTok->type != TokenType::Identifier)
    {
        addError("Expected identifier at start of assignment");
        return nullptr;
    }

    std::string identifier = identTok->lexeme;
    eat();

    skipNewlines();

    if (!expect(TokenType::Assign, "Expected '=' in assignment"))
        return nullptr;

    skipNewlines();

    auto value = parseExpr();
    if (!value)
        return nullptr;

    return std::make_unique<AssignNode>(std::move(identifier), std::move(value));
}

std::unique_ptr<ExprNode> SyntaxParser::parseExpr()
{
    return parseEquality();
}

std::unique_ptr<ExprNode> SyntaxParser::parseEquality()
{
    auto left = parseAdditive();
    if (!left)
        return nullptr;

    while (true)
    {
        if (match(TokenType::Equals))
        {
            skipNewlines();
            auto right = parseAdditive();
            if (!right)
                return nullptr;
            left = std::make_unique<BinaryOpNode>(BinaryOpNode::Operator::Equal, std::move(left), std::move(right));
            skipNewlines();
            continue;
        }

        if (match(TokenType::NotEqual))
        {
            skipNewlines();
            auto right = parseAdditive();
            if (!right)
                return nullptr;
            left = std::make_unique<BinaryOpNode>(BinaryOpNode::Operator::NotEqual, std::move(left), std::move(right));
            skipNewlines();
            continue;
        }

        break;
    }

    return left;
}

std::unique_ptr<ExprNode> SyntaxParser::parseAdditive()
{
    auto left = parseMultiplicative();
    if (!left)
        return nullptr;

    while (true)
    {
        if (match(TokenType::Add))
        {
            skipNewlines();
            auto right = parseMultiplicative();
            if (!right)
                return nullptr;
            left = std::make_unique<BinaryOpNode>(BinaryOpNode::Operator::Add, std::move(left), std::move(right));
            skipNewlines();
            continue;
        }

        if (match(TokenType::Sub))
        {
            skipNewlines();
            auto right = parseMultiplicative();
            if (!right)
                return nullptr;
            left = std::make_unique<BinaryOpNode>(BinaryOpNode::Operator::Sub, std::move(left), std::move(right));
            skipNewlines();
            continue;
        }

        break;
    }

    return left;
}

std::unique_ptr<ExprNode> SyntaxParser::parseMultiplicative()
{
    auto left = parseUnary();
    if (!left)
        return nullptr;

    while (match(TokenType::Mul))
    {
        skipNewlines();
        auto right = parseUnary();
        if (!right)
            return nullptr;
        left = std::make_unique<BinaryOpNode>(BinaryOpNode::Operator::Mul, std::move(left), std::move(right));
        skipNewlines();
    }

    return left;
}

std::unique_ptr<ExprNode> SyntaxParser::parseUnary()
{
    if (match(TokenType::Not))
    {
        skipNewlines();
        auto operand = parseUnary();
        if (!operand)
            return nullptr;
        return std::make_unique<UnaryOpNode>(UnaryOpNode::Operator::LogicalNot, std::move(operand));
    }

    return parsePrimary();
}

std::unique_ptr<ExprNode> SyntaxParser::parsePrimary()
{
    const Token* token = peek();
    if (!token)
    {
        addError("Unexpected end of input while parsing expression");
        return nullptr;
    }

    switch (token->type)
    {
        case TokenType::Identifier:
        {
            std::string name = token->lexeme;
            eat();
            return std::make_unique<IDNode>(std::move(name));
        }

        case TokenType::Number:
        {
            std::int64_t value = std::stoll(token->lexeme);
            eat();
            return std::make_unique<NumberNode>(value);
        }

        case TokenType::True:
        case TokenType::False:
        {
            bool value = (token->type == TokenType::True);
            eat();
            return std::make_unique<BoolLiteralNode>(value);
        }

        default:
            addError("Unexpected token '" + token->lexeme + "' in expression");
            return nullptr;
    }
}

bool SyntaxParser::atEnd() const
{
    return m_index >= m_tokens.size() || (m_tokens[m_index].type == TokenType::EndOfFile);
}

bool SyntaxParser::match(TokenType type)
{
    const Token* token = peek();
    if (token&& token->type == type)
    {
        ++m_index;
        return true;
    }
    return false;
}

bool SyntaxParser::expect(TokenType type, const std::string& message)
{
    if (match(type))
        return true;

    addError(message);
    return false;
}

void SyntaxParser::skipNewlines()
{
    while (match(TokenType::Newline)) {}
}

size_t SyntaxParser::allocateScopeId()
{
    return m_nextScopeId++;
}

void SyntaxParser::addError(const std::string& message)
{
    m_errors.push_back(message);
}
