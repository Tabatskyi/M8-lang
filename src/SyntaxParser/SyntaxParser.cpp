#include "SyntaxParser.hpp"

SyntaxParser::SyntaxParser(std::vector<Token> tokens): _tokens(std::move(tokens)) {}

const Token* SyntaxParser::peek(size_t offset) const
{
    size_t target = _index + offset;
    if (target >= _tokens.size())
        return nullptr;
    return& _tokens[target];
}

const Token* SyntaxParser::eat()
{
    if (atEnd())
        return nullptr;
    return& _tokens[_index++];
}

std::unique_ptr<ProgramNode> SyntaxParser::parseProgram()
{
    ProgramNode::StmtList statements;

    if (atEnd())
        return std::make_unique<ProgramNode>(std::move(statements), 0);

    std::unique_ptr<StmtNode> first = parseStmt();
    if (!first) return nullptr;
    statements.push_back(std::move(first));

    while (match(TokenType::StmtSep))
    {
        if (atEnd()) break;
        std::unique_ptr<StmtNode> next = parseStmt();
        if (!next) return nullptr;
        statements.push_back(std::move(next));
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
        case TokenType::Var:
        case TokenType::Const:
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
    if (!expect(TokenType::Return, "Expected return (ᚷ)"))
        return nullptr;

    skipNewlines();
    if (const Token* t = peek())
    {
        if (t->type == TokenType::Identifier || t->type == TokenType::Number ||
            t->type == TokenType::True || t->type == TokenType::False ||
            t->type == TokenType::LParen || t->type == TokenType::Sub || t->type == TokenType::Not)
        {
            std::unique_ptr<ExprNode> expr = parseExpr();
            if (!expr) return nullptr;
            return std::make_unique<ReturnNode>(std::move(expr));
        }
    }
    return std::make_unique<ReturnNode>(nullptr);
}

std::unique_ptr<IfNode> SyntaxParser::parseIf()
{
    if (!expect(TokenType::If, "Expected if (ᛗ)"))
        return nullptr;

    skipNewlines();
    std::unique_ptr<ExprNode> condition = parseExpr();
    if (!condition)
        return nullptr;

    skipNewlines();
    std::unique_ptr<BlockNode> thenBlock;
    if (match(TokenType::Then))
    {
        skipNewlines();
        std::unique_ptr<StmtNode> thenStmt = parseStmt();
        if (!thenStmt) return nullptr;
        thenBlock = parseBlock(std::move(thenStmt), allocateScopeId());
    }
    else
    {
        thenBlock = std::make_unique<BlockNode>(BlockNode::StmtList{}, allocateScopeId());
    }

    std::unique_ptr<BlockNode> elseBlock;
    if (match(TokenType::Else))
    {
        skipNewlines();
        std::unique_ptr<StmtNode> elseStmt = parseStmt();
        if (!elseStmt) return nullptr;
        elseBlock = parseBlock(std::move(elseStmt), allocateScopeId());
    }

    return std::make_unique<IfNode>(std::move(condition), std::move(thenBlock), std::move(elseBlock));
}

std::unique_ptr<BlockNode> SyntaxParser::parseBlock(std::unique_ptr<StmtNode> stmt, size_t scopeId)
{
    skipNewlines();
    BlockNode::StmtList list;
    list.push_back(std::move(stmt));

    return std::make_unique<BlockNode>(std::move(list), scopeId);
}

std::unique_ptr<DeclNode> SyntaxParser::parseDecl()
{
    bool isMutable = false;
    if (match(TokenType::Var))
        isMutable = true;
    else if (!match(TokenType::Const))
    {
        addError("Expected variable declaration (ᚡ or ᛍ)");
        return nullptr;
    }

    const Token* identTok = peek();
    if (!identTok || identTok->type != TokenType::Identifier)
    {
        addError("Expected identifier after declaration keyword");
        return nullptr;
    }
    std::string identifier = identTok->lexeme;
    eat();

    if (!expect(TokenType::Assign, "Expected assignment colon (᛬) in declaration"))
        return nullptr;

    std::unique_ptr<ExprNode> initializer = parseExpr();
    if (!initializer) return nullptr;

    return std::make_unique<DeclNode>(ValueType::Invalid, std::move(identifier), isMutable, std::move(initializer));
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

    if (match(TokenType::Assign))
    {
        std::unique_ptr<ExprNode> value = parseExpr();
        if (!value) return nullptr;
        return std::make_unique<AssignNode>(std::move(identifier), std::move(value));
    }
    else if (match(TokenType::AddAssign) || match(TokenType::SubAssign) || match(TokenType::MulAssign) || match(TokenType::DivAssign))
    {
        TokenType opTok = _tokens[_index - 1].type;
        std::unique_ptr<ExprNode> rhs = parseExpr();
        if (!rhs)
            return nullptr;

        std::unique_ptr<ExprNode> leftId = std::make_unique<IDNode>(identifier);
        BinaryOpNode::Operator bop = BinaryOpNode::Operator::Add;
        switch (opTok)
        {
            case TokenType::AddAssign: bop = BinaryOpNode::Operator::Add; break;
            case TokenType::SubAssign: bop = BinaryOpNode::Operator::Sub; break;
            case TokenType::MulAssign: bop = BinaryOpNode::Operator::Mul; break;
            case TokenType::DivAssign: bop = BinaryOpNode::Operator::Div; break;
            default: break;
        }
        std::unique_ptr<BinaryOpNode> bin = std::make_unique<BinaryOpNode>(bop, std::move(leftId), std::move(rhs));
        return std::make_unique<AssignNode>(std::move(identifier), std::move(bin));
    }

    addError("Expected assignment operator (᛬ or op᛬)");
    return nullptr;
}

std::unique_ptr<ExprNode> SyntaxParser::parseExpr()
{
    return parseEquality();
}

std::unique_ptr<ExprNode> SyntaxParser::parseEquality()
{
    std::unique_ptr<ExprNode> left = parseAdditive();
    if (!left)
        return nullptr;

    while (true)
    {
        if (match(TokenType::Equals))
        {
            skipNewlines();
            std::unique_ptr<ExprNode> right = parseAdditive();
            if (!right)
                return nullptr;
            left = std::make_unique<BinaryOpNode>(BinaryOpNode::Operator::Equal, std::move(left), std::move(right));
            skipNewlines();
            continue;
        }

        if (match(TokenType::NotEqual))
        {
            skipNewlines();
            std::unique_ptr<ExprNode> right = parseAdditive();
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
    std::unique_ptr<ExprNode> left = parseMultiplicative();
    if (!left)
        return nullptr;

    while (true)
    {
        if (match(TokenType::Add))
        {
            skipNewlines();
            std::unique_ptr<ExprNode> right = parseMultiplicative();
            if (!right)
                return nullptr;
            left = std::make_unique<BinaryOpNode>(BinaryOpNode::Operator::Add, std::move(left), std::move(right));
            skipNewlines();
            continue;
        }

        if (match(TokenType::Sub))
        {
            skipNewlines();
            std::unique_ptr<ExprNode> right = parseMultiplicative();
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
    std::unique_ptr<ExprNode> left = parseUnary();
    if (!left)
        return nullptr;

    while (true)
    {
        if (match(TokenType::Mul))
        {
            std::unique_ptr<ExprNode> right = parseUnary();
            if (!right) return nullptr;
            left = std::make_unique<BinaryOpNode>(BinaryOpNode::Operator::Mul, std::move(left), std::move(right));
            continue;
        }
        if (match(TokenType::Div))
        {
            std::unique_ptr<ExprNode> right = parseUnary();
            if (!right) return nullptr;
            left = std::make_unique<BinaryOpNode>(BinaryOpNode::Operator::Div, std::move(left), std::move(right));
            continue;
        }
        break;
    }

    return left;
}

std::unique_ptr<ExprNode> SyntaxParser::parseUnary()
{
    if (match(TokenType::Not))
    {
        skipNewlines();
        std::unique_ptr<ExprNode> operand = parseUnary();
        if (!operand)
            return nullptr;
        return std::make_unique<UnaryOpNode>(UnaryOpNode::Operator::LogicalNot, std::move(operand));
    }

    if (match(TokenType::Sub))
    {
        std::unique_ptr<ExprNode> operand = parseUnary();
        if (!operand) return nullptr;
        std::unique_ptr<NumberNode> zero = std::make_unique<NumberNode>(0);
        return std::make_unique<BinaryOpNode>(BinaryOpNode::Operator::Sub, std::move(zero), std::move(operand));
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
            std::string text = token->lexeme;
            if (text.size() >= 3 && (text.substr(text.size()-3) == "ᛰ" || text.substr(text.size()-3) == "ᛯ"))
                text.resize(text.size()-3);
            std::int64_t value = std::stoll(text);
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
    return _index >= _tokens.size() || (_tokens[_index].type == TokenType::EndOfFile);
}

bool SyntaxParser::match(TokenType type)
{
    const Token* token = peek();
    if (token&& token->type == type)
    {
        ++_index;
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
    return _nextScopeId++;
}

void SyntaxParser::addError(const std::string& message)
{
    _errors.push_back(message);
}