#include "SyntaxParser.hpp"

SyntaxParser::SyntaxParser(std::vector<Token> tokens): _tokens(std::move(tokens))
{
    for (size_t i = 0; i + 1 < _tokens.size(); ++i)
    {
        if (_tokens[i].type == TokenType::Struct && _tokens[i + 1].type == TokenType::Identifier)
            _knownStructs.insert(_tokens[i + 1].lexeme);
    }
}

const Token* SyntaxParser::peek(size_t offset) const
{
    size_t target = _index + offset;
    if (target >= _tokens.size())
        return nullptr;
    return &_tokens[target];
}

const Token* SyntaxParser::eat()
{
    if (atEnd())
        return nullptr;
    return &_tokens[_index++];
}

std::unique_ptr<ProgramNode> SyntaxParser::parseProgram()
{
    ProgramNode::StmtList statements;

    if (atEnd())
        return std::make_unique<ProgramNode>(std::move(statements), 0);

    while (!atEnd())
    {
        skipNewlines();
        while (match(TokenType::StmtSep))
            skipNewlines();
        if (atEnd())
            break;
        std::unique_ptr<StmtNode> currentStmt = parseStmt();
        if (!currentStmt) 
            return nullptr;
        statements.push_back(std::move(currentStmt));
        if (!match(TokenType::StmtSep))
            break;
    }

    skipNewlines();
    while (match(TokenType::StmtSep))
        skipNewlines();

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
            if (identifierStartsAssignment())
                return parseAssign();
            return parseExprStmt();

        case TokenType::Return:
            return parseReturn();

        case TokenType::If:
            return parseIf();

        case TokenType::Function:
            return parseFunction();

        case TokenType::Struct:
            return parseStruct();

        default:
            if (canStartExprStatement(token))
                return parseExprStmt();
            addError("Unexpected token '" + token->lexeme + "' at start of statement");
            return nullptr;
    }
}

std::unique_ptr<FunctionNode> SyntaxParser::parseFunction(const std::string& masterStruct)
{
    bool isMethodKeyword = match(TokenType::Method);
    bool isFunctionKeyword = false;
    if (!isMethodKeyword)
        isFunctionKeyword = match(TokenType::Function);
    else
        isFunctionKeyword = true;

    if (!isMethodKeyword && !isFunctionKeyword)
    {
        addError("Expected function (ᚠ) or method (ᛃ) declaration keyword");
        return nullptr;
    }

    if (isMethodKeyword && masterStruct.empty())
    {
        addError("Method declaration is only allowed inside a struct body");
        return nullptr;
    }

    bool isMember = isMethodKeyword || !masterStruct.empty();

    const Token* identTok = peek();
    if (!identTok || identTok->type != TokenType::Identifier)
    {
        addError("Expected identifier after function keyword");
        return nullptr;
    }
    std::string name = identTok->lexeme; eat();

    if (!expect(TokenType::Assign, "Expected '᛬' after function name")) 
        return nullptr;
    if (!expect(TokenType::LParen, "Expected '(' after '᛬' in function decl"))
        return nullptr;

    std::vector<FunctionNode::Param> params;
    bool isTemplate = false;
    if (isMember)
        params.push_back(FunctionNode::Param{ TypeDesc::Struct(masterStruct), std::string("_self") });

    if (peek() && peek()->type == TokenType::Identifier)
    {
        while (true)
        {
            const Token* pTok = peek();
            if (!pTok || pTok->type != TokenType::Identifier)
            {
                addError("Expected parameter identifier");
                return nullptr;
            }
            std::string pName = pTok->lexeme; eat();
            if (!expect(TokenType::Assign, "Expected '᛬' after parameter name")) return nullptr;
            TypeDesc pType = parseTypeDesc();
            if (pType.kind == TypeDesc::Kind::TemplateParam)
                isTemplate = true;
            params.push_back(FunctionNode::Param{ pType, pName });
            if (!match(TokenType::StmtSep)) break;
        }
    }

    if (!expect(TokenType::RParen, "Expected ')' after parameter list")) 
        return nullptr;
    if (!expect(TokenType::Assign, "Expected '᛬' after ')' for return type")) 
        return nullptr;
    TypeDesc returnType = parseTypeDesc();
    if (returnType.kind == TypeDesc::Kind::TemplateParam)
        isTemplate = true;
    if (!expect(TokenType::Assign, "Expected '᛬' before function body")) 
        return nullptr;

    std::vector<std::unique_ptr<StmtNode>> bodyStatements;
    while (!atEnd())
    {
        skipNewlines();
        const Token* nextToken = peek();
        if (!nextToken || nextToken->type == TokenType::StmtSep || nextToken->type == TokenType::Function || nextToken->type == TokenType::Struct)
            break;
        std::unique_ptr<StmtNode> stmt = parseStmt();
        if (!stmt)
            return nullptr;
        bodyStatements.push_back(std::move(stmt));
        skipNewlines();
    }

    auto body = parseBlock(std::move(bodyStatements), allocateScopeId());

    return std::make_unique<FunctionNode>(std::move(name), std::move(params), returnType, std::move(body), body->scopeId(), isMember ? masterStruct : std::string{}, isTemplate);
}

std::unique_ptr<StructDeclNode> SyntaxParser::parseStruct()
{
    if (!expect(TokenType::Struct, "Expected struct declaration keyword (ᛋ)"))
        return nullptr;
    const Token* identTok = peek();
    if (!identTok || identTok->type != TokenType::Identifier)
    {
        addError("Expected struct name after ᛋ");
        return nullptr;
    }
    std::string name = identTok->lexeme; eat();
    if (!expect(TokenType::Assign, "Expected '᛬' after struct name"))
        return nullptr;

    std::vector<StructDeclNode::Field> fields;
    std::vector<std::unique_ptr<FunctionNode>> methods;

    bool methodsPending = false;
    if (peek() && peek()->type == TokenType::Identifier)
    {
        while (true)
        {
            const Token* fTok = peek();
            if (!fTok || fTok->type != TokenType::Identifier)
            {
                addError("Expected field identifier in struct declaration");
                return nullptr;
            }
            std::string fieldName = fTok->lexeme; eat();
            if (!expect(TokenType::Assign, "Expected '᛬' after field name"))
                return nullptr;
            TypeDesc fieldType = parseTypeDesc();
            fields.push_back(StructDeclNode::Field{ fieldType, fieldName, true });
            
            if (match(TokenType::StmtSep))
            {
                if (peek() && (peek()->type == TokenType::Function || peek()->type == TokenType::Method))
                {
                    methodsPending = true;
                    break;
                }
            }
            else
            {
                break;
            }
        }
    }

    if (methodsPending || match(TokenType::StmtSep))
    {
        while (peek() && (peek()->type == TokenType::Function || peek()->type == TokenType::Method))
        {
            std::unique_ptr<FunctionNode> method = parseFunction(name);
            if (!method)
                return nullptr;
            methods.push_back(std::move(method));
            if (!match(TokenType::StmtSep))
                break;
        }
    }

    return std::make_unique<StructDeclNode>(std::move(name), std::move(fields), std::move(methods));
}

std::unique_ptr<ReturnNode> SyntaxParser::parseReturn()
{
    if (!expect(TokenType::Return, "Expected return (ᚷ)"))
        return nullptr;

    skipNewlines();
    const Token* next = peek();
    if (canStartExprStatement(next))
    {
        std::unique_ptr<ExprNode> expr = parseExpr();
        if (!expr)
            return nullptr;
        return std::make_unique<ReturnNode>(std::move(expr));
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
        std::vector<std::unique_ptr<StmtNode>> thenStatements;
        std::unique_ptr<StmtNode> thenStmt = parseStmt();
        if (!thenStmt) return nullptr;
        thenStatements.push_back(std::move(thenStmt));
        thenBlock = parseBlock(std::move(thenStatements), allocateScopeId());
    }
    else
    {
        thenBlock = std::make_unique<BlockNode>(BlockNode::StmtList{}, allocateScopeId());
    }

    std::unique_ptr<BlockNode> elseBlock;
    if (match(TokenType::Else))
    {
        skipNewlines();
        std::vector<std::unique_ptr<StmtNode>> elseStatements;
        std::unique_ptr<StmtNode> elseStmt = parseStmt();
        if (!elseStmt) return nullptr;
        elseStatements.push_back(std::move(elseStmt));
        elseBlock = parseBlock(std::move(elseStatements), allocateScopeId());
    }

    return std::make_unique<IfNode>(std::move(condition), std::move(thenBlock), std::move(elseBlock));
}

std::unique_ptr<BlockNode> SyntaxParser::parseBlock(std::vector<std::unique_ptr<StmtNode>> statements, size_t scopeId)
{
    skipNewlines();
    return std::make_unique<BlockNode>(std::move(statements), scopeId);
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

    std::unique_ptr<ExprNode> initializer;
    TypeDesc declaredType = TypeDesc::Builtin(ValueType::Invalid);

    if (isStructLiteralAhead())
    {
        initializer = parseStructLiteral();
    }
    else
    {
        size_t savedIndex = _index;
        if (canStartType(peek()))
        {
            TypeDesc potentialType = parseTypeDesc();
            if (match(TokenType::Assign))
            {
                declaredType = potentialType;
                initializer = parseExpr();
            }
            else
            {
                _index = savedIndex;
            }
        }

        if (!initializer)
            initializer = parseExpr();
    }

    if (!initializer)
        return nullptr;

    if (declaredType.kind == TypeDesc::Kind::Builtin && declaredType.builtin == ValueType::Invalid)
    {
        if (auto* literal = dynamic_cast<StructLiteralNode*>(initializer.get()))
            declaredType = literal->structType();
    }

    std::vector<std::unique_ptr<ExprNode>> inits;
    inits.push_back(std::move(initializer));
    return std::make_unique<DeclNode>(declaredType, std::move(identifier), isMutable, std::move(inits));
}

TypeDesc SyntaxParser::parseTypeDesc()
{
    const Token* token = peek();
    if (!token)
    {
        addError("Expected type specifier");
        return TypeDesc::Builtin(ValueType::Invalid);
    }

    switch (token->type)
    {
    case TokenType::I32: eat(); return TypeDesc::Builtin(ValueType::I32);
    case TokenType::I64: eat(); return TypeDesc::Builtin(ValueType::I64);
    case TokenType::Bool: eat(); return TypeDesc::Builtin(ValueType::Bool);
    case TokenType::String: eat(); return TypeDesc::Builtin(ValueType::String);
    case TokenType::TemplateType: eat(); return TypeDesc::TemplateParam("ᛸ");
    case TokenType::Identifier: 
    {
        std::string name = token->lexeme; eat();
        return TypeDesc::Struct(name);
    }
    default:
        addError("Expected type specifier");
        return TypeDesc::Builtin(ValueType::Invalid);
    }
}

std::unique_ptr<StmtNode> SyntaxParser::parseAssign()
{
    const Token* identTok = peek();
    if (!identTok || identTok->type != TokenType::Identifier)
    {
        addError("Expected identifier at start of assignment");
        return nullptr;
    }

    std::string identifier = identTok->lexeme;
    eat();

    std::vector<std::string> chain;
    skipNewlines();
    while (match(TokenType::Dot))
    {
        const Token* fieldTok = peek();
        if (!fieldTok || fieldTok->type != TokenType::Identifier)
        {
            addError("Expected field name after '.' in assignment");
            return nullptr;
        }
        chain.push_back(fieldTok->lexeme);
        eat();
        skipNewlines();
    }

    if (!chain.empty())
    {
        if (!expect(TokenType::Assign, "Expected '᛬' in field assignment"))
            return nullptr;
        skipNewlines();
        std::unique_ptr<ExprNode> value = parseExpr();
        if (!value)
            return nullptr;
        auto target = std::make_unique<FieldAccessNode>(std::move(identifier), std::move(chain));
        return std::make_unique<AssignFieldNode>(std::move(target), std::move(value));
    }

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

std::unique_ptr<StmtNode> SyntaxParser::parseExprStmt()
{
    std::unique_ptr<ExprNode> expr = parseExpr();
    if (!expr)
        return nullptr;
    return std::make_unique<ExprStmtNode>(std::move(expr));
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
            if (isStructLiteralAhead())
                return parseStructLiteral();

            std::string name = token->lexeme;
            eat();

            if (match(TokenType::LParen))
            {
                std::vector<std::unique_ptr<ExprNode>> args;
                if (!match(TokenType::RParen))
                {
                    while (true)
                    {
                        std::unique_ptr<ExprNode> arg = parseExpr();
                        if (!arg) 
                            return nullptr;
                        args.push_back(std::move(arg));

                        if (match(TokenType::RParen)) 
                            break;
                        if (!expect(TokenType::StmtSep, "Expected 'ᛵ' between function arguments"))
                            return nullptr;
                    }
                }
                return std::make_unique<FunctionCallNode>(std::move(name), std::move(args));
            }

            skipNewlines();
            if (peek() && peek()->type == TokenType::Dot)
            {
                std::vector<std::string> chain;
                while (match(TokenType::Dot))
                {
                    const Token* fieldTok = peek();
                    if (!fieldTok || fieldTok->type != TokenType::Identifier)
                    {
                        addError("Expected field name after '.' in expression");
                        return nullptr;
                    }
                    chain.push_back(fieldTok->lexeme);
                    eat();
                    skipNewlines();
                }

                if (!chain.empty() && peek() && peek()->type == TokenType::LParen)
                {
                    std::string fnName = chain.back();
                    chain.pop_back();
                    if (!expect(TokenType::LParen, "Expected '(' after member function name"))
                        return nullptr;
                    std::vector<std::unique_ptr<ExprNode>> args;
                    if (!match(TokenType::RParen))
                    {
                        while (true)
                        {
                            std::unique_ptr<ExprNode> arg = parseExpr();
                            if (!arg) return nullptr;
                            args.push_back(std::move(arg));
                            if (match(TokenType::RParen))
                                break;
                            if (!expect(TokenType::StmtSep, "Expected 'ᛵ' between member function arguments"))
                                return nullptr;
                        }
                    }
                    return std::make_unique<MemberFunctionCallNode>(std::move(name), std::move(chain), std::move(fnName), std::move(args));
                }

                if (peek() && peek()->type == TokenType::LParen)
                {
                    if (!chain.empty())
                    {
                        addError("Callable object syntax is only supported on variables, not nested fields");
                        return nullptr;
                    }
                    if (!expect(TokenType::LParen, "Expected '(' after callable object"))
                        return nullptr;
                    std::vector<std::unique_ptr<ExprNode>> args;
                    if (!match(TokenType::RParen))
                    {
                        while (true)
                        {
                            std::unique_ptr<ExprNode> arg = parseExpr();
                            if (!arg) return nullptr;
                            args.push_back(std::move(arg));
                            if (match(TokenType::RParen))
                                break;
                            if (!expect(TokenType::StmtSep, "Expected 'ᛵ' between callable arguments"))
                                return nullptr;
                        }
                    }
                    return std::make_unique<MemberFunctionCallNode>(std::move(name), std::vector<std::string>{}, std::string("call"), std::move(args));
                }

                if (chain.empty())
                {
                    addError("Expected member function call or field name after '.'");
                    return nullptr;
                }

                return std::make_unique<FieldAccessNode>(std::move(name), std::move(chain));
            }

            return std::make_unique<IDNode>(std::move(name));
        }

        case TokenType::Read:
        case TokenType::Write:
        {
            TokenType ioType = token->type; eat();
            if (!expect(TokenType::LParen, "Expected '(' after builtin I/O token"))
                return nullptr;
            std::vector<std::unique_ptr<ExprNode>> args;
            if (!match(TokenType::RParen))
            {
                std::unique_ptr<ExprNode> arg = parseExpr();
                if (!arg) return nullptr;
                args.push_back(std::move(arg));
                if (!expect(TokenType::RParen, "Expected ')' to close builtin I/O call"))
                    return nullptr;
            }
            std::string name = (ioType == TokenType::Read) ? "__builtin_read" : "__builtin_write";
            return std::make_unique<FunctionCallNode>(std::move(name), std::move(args));
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

        case TokenType::StringLiteral:
        {
            std::string value = token->lexeme;
            eat();
            return std::make_unique<StringLiteralNode>(std::move(value));
        }

        case TokenType::LParen:
        {
            eat();
            std::unique_ptr<ExprNode> inner = parseExpr();
            if (!inner) 
                return nullptr;
            if (!expect(TokenType::RParen, "Expected ')' after expression")) 
                return nullptr;
            return inner;
        }

        default:
            addError("Unexpected token '" + token->lexeme + "' in expression");
            return nullptr;
    }
}

std::unique_ptr<ExprNode> SyntaxParser::parseStructLiteral()
{
    const Token* typeTok = peek();
    if (!typeTok || typeTok->type != TokenType::Identifier)
    {
        addError("Struct literal requires a struct type name");
        return nullptr;
    }

    std::string structName = typeTok->lexeme;
    eat();
    TypeDesc literalType = TypeDesc::Struct(structName);

    if (!expect(TokenType::LParen, "Expected '(' after struct literal type"))
        return nullptr;

    std::vector<std::unique_ptr<ExprNode>> args;
    if (!match(TokenType::RParen))
    {
        while (true)
        {
            std::unique_ptr<ExprNode> arg = parseExpr();
            if (!arg)
                return nullptr;
            args.push_back(std::move(arg));
            if (match(TokenType::RParen))
                break;
            if (!expect(TokenType::StmtSep, "Expected 'ᛵ' between struct literal arguments"))
                return nullptr;
        }
    }

    return std::make_unique<StructLiteralNode>(std::move(literalType), std::move(args));
}

bool SyntaxParser::atEnd() const
{
    return _index >= _tokens.size() || (_tokens[_index].type == TokenType::EndOfFile);
}

bool SyntaxParser::match(TokenType type)
{
    const Token* token = peek();
    if (token && token->type == type)
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

bool SyntaxParser::canStartType(const Token* token) const
{
    if (!token)
        return false;
    switch (token->type)
    {
    case TokenType::I32:
    case TokenType::I64:
    case TokenType::Bool:
    case TokenType::String:
    case TokenType::TemplateType:
    case TokenType::Identifier:
        return true;
    default:
        return false;
    }
}

bool SyntaxParser::canStartExprStatement(const Token* token) const
{
    if (!token)
        return false;
    switch (token->type)
    {
    case TokenType::Identifier:
    case TokenType::Number:
    case TokenType::True:
    case TokenType::False:
    case TokenType::StringLiteral:
    case TokenType::Read:
    case TokenType::Write:
    case TokenType::LParen:
    case TokenType::Sub:
    case TokenType::Not:
        return true;
    default:
        return false;
    }
}

bool SyntaxParser::identifierStartsAssignment() const
{
    const Token* first = peek();
    if (!first || first->type != TokenType::Identifier)
        return false;

    size_t offset = 1;
    while (true)
    {
        const Token* look = peek(offset);
        if (!look)
            return false;

        if (look->type == TokenType::Newline)
        {
            ++offset;
            continue;
        }

        if (look->type == TokenType::Dot)
        {
            const Token* afterDot = peek(offset + 1);
            if (!afterDot || afterDot->type != TokenType::Identifier)
                return false;
            offset += 2;
            continue;
        }

        switch (look->type)
        {
        case TokenType::Assign:
        case TokenType::AddAssign:
        case TokenType::SubAssign:
        case TokenType::MulAssign:
        case TokenType::DivAssign:
            return true;
        default:
            return false;
        }
    }
}

bool SyntaxParser::isStructLiteralAhead() const
{
    const Token* token = peek();
    if (!token || token->type != TokenType::Identifier)
        return false;
    if (!_knownStructs.count(token->lexeme))
        return false;
    const Token* next = peek(1);
    return next && next->type == TokenType::LParen;
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