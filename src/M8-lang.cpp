#include "M8-lang.hpp"

using std::string;

struct IRContext
{
    int tempId = 0;
    std::ostringstream ir;
};

struct VariableInfo
{
    ValueType type;
    bool isMutable;
    string name;
    size_t scopeId;
};

static const std::unordered_map<string, TokenType> keywordMap =
{
    {"var", TokenType::Var},
    {"mut", TokenType::Mut},
    {"return", TokenType::Return},
    {"if", TokenType::If},
    {"else", TokenType::Else},
    {"i32", TokenType::I32},
    {"i64", TokenType::I64},
    {"bool", TokenType::Bool},
    {"true", TokenType::True},
    {"false", TokenType::False}
};

TokenType classifyIdentifier(const string& ident)
{
    auto it = keywordMap.find(ident);
    return (it != keywordMap.end()) ? it->second : TokenType::Identifier;
}

std::vector<Token> lexSource(const string& source)
{
    std::vector<Token> out;
    enum class State { Start, Identifier, Number };
    State state = State::Start;
    string buffer;

    for (size_t i = 0; i <= source.size(); )
    {
        char c = (i < source.size()) ? source[i] : '\0';
        bool atEnd = (i == source.size());
        TokenType kind;

        switch (state)
        {
        case State::Start:
            if (atEnd)
            {
                ++i;
                continue;
            }
            if (c == '\n')
            {
                out.push_back(Token{ "\n", TokenType::Newline });
                ++i;
                continue;
            }
            if (std::isspace(static_cast<unsigned char>(c)))
            {
                ++i;
                continue;
            }
            if (c == '/' && i + 1 < source.size() && source[i + 1] == '/')
            {
                i += 2;
                while (i < source.size() && source[i] != '\n') ++i;
                continue;
            }
            if (c == '=' && i + 1 < source.size() && source[i + 1] == '=')
            {
                out.push_back(Token{ "==", TokenType::Equals });
                i += 2;
                continue;
            }
            if (c == '!' && i + 1 < source.size() && source[i + 1] == '=')
            {
                out.push_back(Token{ "!=", TokenType::NotEqual });
                i += 2;
                continue;
            }
            if (c == '!')
            {
                out.push_back(Token{ "!", TokenType::Not });
                ++i;
                continue;
            }
            if (std::isalpha(static_cast<unsigned char>(c)) || c == '_')
            {
                buffer.assign(1, c);
                state = State::Identifier;
                ++i;
                continue;
            }
            if (std::isdigit(static_cast<unsigned char>(c)))
            {
                buffer.assign(1, c);
                state = State::Number;
                ++i;
                continue;
            }

            kind = TokenType::Newline;
            switch (c)
            {
            case '{': kind = TokenType::BlockStart; break;
            case '}': kind = TokenType::BlockEnd; break;
            case '=': kind = TokenType::Assign; break;
            case '+': kind = TokenType::Add; break;
            case '-': kind = TokenType::Sub; break;
            case '*': kind = TokenType::Mul; break;
            default: ++i; continue;
            }
            out.push_back(Token{ string(1, c), kind });
            ++i;
            continue;

        case State::Identifier:
            if (!atEnd && (std::isalnum(static_cast<unsigned char>(c)) || c == '_'))
            {
                buffer.push_back(c);
                ++i;
                continue;
            }
            out.push_back(Token{ buffer, classifyIdentifier(buffer) });
            buffer.clear();
            state = State::Start;
            continue;

        case State::Number:
            if (!atEnd && std::isdigit(static_cast<unsigned char>(c)))
            {
                buffer.push_back(c);
                ++i;
                continue;
            }
            out.push_back(Token{ buffer, TokenType::Number });
            buffer.clear();
            state = State::Start;
            continue;
        }
    }

    out.push_back(Token{ "", TokenType::EndOfFile });
    return out;
}

static string typeToString(ValueType type)
{
    switch (type)
    {
    case ValueType::I32: return "i32";
    case ValueType::I64: return "i64";
    case ValueType::Bool: return "bool";
    default: return "<invalid>";
    }
}

static bool isNumeric(ValueType type)
{
    return type == ValueType::I32 || type == ValueType::I64;
}

static ValueType widerType(ValueType lhs, ValueType rhs)
{
    if (!isNumeric(lhs) || !isNumeric(rhs))
        return ValueType::Invalid;
    if (lhs == ValueType::I64 || rhs == ValueType::I64)
        return ValueType::I64;
    return ValueType::I32;
}

static ValueType comparisonOperandType(ValueType lhs, ValueType rhs)
{
    if (lhs == ValueType::Bool && rhs == ValueType::Bool)
        return ValueType::Bool;
    return widerType(lhs, rhs);
}

static bool isAssignable(ValueType target, ValueType source)
{
    if (target == ValueType::Invalid || source == ValueType::Invalid)
        return false;
    if (target == source)
        return true;
    if (target == ValueType::I64 && source == ValueType::I32)
        return true;
    return false;
}

static bool canConvertToI32(ValueType type)
{
    return type == ValueType::I32 || type == ValueType::I64 || type == ValueType::Bool;
}

class SemanticAnalyzer : public ASTVisitor
{
public:
    bool analyze(const ProgramNode& program)
    {
        m_errors.clear();
        m_warnings.clear();
        m_symbols.clear();
        m_scopeSymbols.clear();
        m_scopeStack.clear();
        m_nextSymbolId = 0;
        m_returnSeen = false;

        program.accept(*this);

        if (!m_returnSeen)
            addWarning("Missing return statement; defaulting to 'return 0'.");

        return m_errors.empty();
    }

    const std::vector<string>& errors() const { return m_errors; }
    const std::vector<string>& warnings() const { return m_warnings; }
    const std::unordered_map<SymbolID, VariableInfo>& symbols() const { return m_symbols; }

    void visitProgram(const ProgramNode& node) override
    {
        enterScope(node.scopeId());
        for (const auto& stmt : node.statements())
        {
            if (stmt)
                stmt->accept(*this);
        }
        exitScope();
    }

    void visitBlock(const BlockNode& node) override
    {
        enterScope(node.scopeId());
        for (const auto& stmt : node.statements())
        {
            if (stmt)
                stmt->accept(*this);
        }
        exitScope();
    }

    void visitDecl(const DeclNode& node) override
    {
        const string& name = node.identifier();
        size_t scope = currentScopeId();
        auto& scopeMap = m_scopeSymbols[scope];
        if (scopeMap.count(name))
        {
            addError("Variable '" + name + "' redeclared");
            return;
        }

        if (const ExprNode* init = node.initializer())
        {
            init->accept(*this);
            ValueType initType = init->type();
            if (!isAssignable(node.declaredType(), initType))
            {
                addError("Cannot initialize '" + name + "' of type " + typeToString(node.declaredType()) +
                    " with value of type " + typeToString(initType));
            }
        }

        SymbolID symbolId = m_nextSymbolId++;
        scopeMap.emplace(name, symbolId);
        m_symbols.emplace(symbolId, VariableInfo{ node.declaredType(), node.isMutable(), name, scope });
        node.setSymbolId(symbolId);
    }

    void visitAssign(const AssignNode& node) override
    {
        SymbolID symbolId = resolveSymbol(node.identifier());
        if (symbolId == InvalidSymbolID)
        {
            addError("Assignment to undeclared variable '" + node.identifier() + "'");
        }
        else
        {
            const auto& info = m_symbols[symbolId];
            if (!info.isMutable)
                addError("Variable '" + node.identifier() + "' is immutable");
            node.setSymbolId(symbolId);
        }

        if (const ExprNode* value = node.value())
        {
            value->accept(*this);
            if (symbolId != InvalidSymbolID)
            {
                const auto& info = m_symbols[symbolId];
                ValueType valueType = value->type();
                if (!isAssignable(info.type, valueType))
                {
                    addError("Cannot assign value of type " + typeToString(valueType) +
                        " to variable '" + node.identifier() + "' of type " + typeToString(info.type));
                }
            }
        }
    }

    void visitIf(const IfNode& node) override
    {
        if (const ExprNode* cond = node.condition())
        {
            cond->accept(*this);
            if (cond->type() != ValueType::Bool)
                addError("Condition of if statement must be bool");
        }

        if (const BlockNode* thenBlock = node.thenBlock())
            thenBlock->accept(*this);
        if (const BlockNode* elseBlock = node.elseBlock())
            elseBlock->accept(*this);
    }

    void visitReturn(const ReturnNode& node) override
    {
        m_returnSeen = true;
        if (!node.expr())
        {
            addError("Return statement requires an expression");
            return;
        }

        node.expr()->accept(*this);
        ValueType type = node.expr()->type();
        if (!canConvertToI32(type))
            addError("Return type must be convertible to i32, got " + typeToString(type));
    }

    void visitBinaryOp(const BinaryOpNode& node) override
    {
        if (const ExprNode* left = node.left())
            left->accept(*this);
        if (const ExprNode* right = node.right())
            right->accept(*this);

        const ExprNode* left = node.left();
        const ExprNode* right = node.right();
        ValueType leftType = left ? left->type() : ValueType::Invalid;
        ValueType rightType = right ? right->type() : ValueType::Invalid;

        if (leftType == ValueType::Invalid || rightType == ValueType::Invalid)
        {
            node.setType(ValueType::Invalid);
            return;
        }

        switch (node.op())
        {
        case BinaryOpNode::Operator::Add:
        case BinaryOpNode::Operator::Sub:
        case BinaryOpNode::Operator::Mul:
        {
            if (!isNumeric(leftType) || !isNumeric(rightType))
            {
                addError("Arithmetic operators require numeric operands");
                node.setType(ValueType::Invalid);
                return;
            }
            node.setType(widerType(leftType, rightType));
            return;
        }
        case BinaryOpNode::Operator::Equal:
        case BinaryOpNode::Operator::NotEqual:
        {
            ValueType operandType = comparisonOperandType(leftType, rightType);
            if (operandType == ValueType::Invalid)
            {
                addError("Comparison requires compatible operand types");
                node.setType(ValueType::Invalid);
                return;
            }
            node.setType(ValueType::Bool);
            return;
        }
        }

        node.setType(ValueType::Invalid);
    }

    void visitUnaryOp(const UnaryOpNode& node) override
    {
        if (const ExprNode* operand = node.operand())
            operand->accept(*this);

        const ExprNode* operand = node.operand();
        ValueType operandType = operand ? operand->type() : ValueType::Invalid;
        if (operandType != ValueType::Bool)
        {
            addError("Logical not operator requires bool operand");
            node.setType(ValueType::Invalid);
            return;
        }

        node.setType(ValueType::Bool);
    }

    void visitID(const IDNode& node) override
    {
        SymbolID symbolId = resolveSymbol(node.name());
        if (symbolId == InvalidSymbolID)
        {
            addError("Use of undeclared variable '" + node.name() + "'");
            node.setType(ValueType::Invalid);
            return;
        }
        node.setSymbolId(symbolId);
        node.setType(m_symbols[symbolId].type);
    }

    void visitNumber(const NumberNode& node) override
    {
        std::int64_t value = node.value();
        if (value >= std::numeric_limits<std::int32_t>::min() && value <= std::numeric_limits<std::int32_t>::max())
            node.setType(ValueType::I32);
        else
            node.setType(ValueType::I64);
    }

    void visitBoolLiteral(const BoolLiteralNode& node) override
    {
        node.setType(ValueType::Bool);
    }

private:
    void enterScope(size_t scopeId)
    {
        m_scopeStack.push_back(scopeId);
        m_scopeSymbols.try_emplace(scopeId, std::unordered_map<string, SymbolID>{});
    }

    void exitScope()
    {
        if (!m_scopeStack.empty())
            m_scopeStack.pop_back();
    }

    size_t currentScopeId() const
    {
        return m_scopeStack.empty() ? 0 : m_scopeStack.back();
    }

    SymbolID resolveSymbol(const string& name) const
    {
        for (auto it = m_scopeStack.rbegin(); it != m_scopeStack.rend(); ++it)
        {
            auto scopeIt = m_scopeSymbols.find(*it);
            if (scopeIt == m_scopeSymbols.end())
                continue;

            auto symIt = scopeIt->second.find(name);
            if (symIt != scopeIt->second.end())
                return symIt->second;
        }
        return InvalidSymbolID;
    }

    void addError(const string& message)
    {
        m_errors.push_back(message);
    }

    void addWarning(const string& message)
    {
        m_warnings.push_back(message);
    }

    std::unordered_map<SymbolID, VariableInfo> m_symbols;
    std::unordered_map<size_t, std::unordered_map<string, SymbolID>> m_scopeSymbols;
    std::vector<size_t> m_scopeStack;
    SymbolID m_nextSymbolId = 0;
    std::vector<string> m_errors;
    std::vector<string> m_warnings;
    bool m_returnSeen = false;
};

struct CodegenValue
{
    string operand;
    ValueType type;
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
    CodeGenerator(IRContext& ctx, const std::unordered_map<SymbolID, VariableInfo>& symbols)
        : m_ctx(ctx)
    {
        for (const auto& [id, info] : symbols)
        {
            CodegenVariable var;
            var.type = info.type;
            var.isMutable = info.isMutable;
            var.pointer = "%" + info.name + "." + std::to_string(id);
            m_variables.emplace(id, std::move(var));
        }
    }

    void generate(const ProgramNode& program)
    {
        m_currentBlockTerminated = false;
        program.accept(*this);
        if (!m_currentBlockTerminated)
            emitReturn({ "0", ValueType::I32 });
    }

    void visitProgram(const ProgramNode& node) override
    {
        for (const auto& stmt : node.statements())
        {
            if (m_currentBlockTerminated)
                break;
            if (stmt)
                stmt->accept(*this);
        }
    }

    void visitBlock(const BlockNode& node) override
    {
        generateBlock(node, "");
    }

    void visitDecl(const DeclNode& node) override
    {
        SymbolID symbolId = node.symbolId();
        if (symbolId == InvalidSymbolID)
            return;

        CodegenVariable& var = getVariable(symbolId);
        ensureAllocated(var);

        CodegenValue value{ zeroLiteral(var.type), var.type };
        if (const ExprNode* init = node.initializer())
        {
            init->accept(*this);
            value = popValue();
            value = ensureType(std::move(value), var.type);
        }

        storeValue(var, value);
    }

    void visitAssign(const AssignNode& node) override
    {
        SymbolID symbolId = node.symbolId();
        if (symbolId == InvalidSymbolID)
            return;

        CodegenVariable& var = getVariable(symbolId);
        ensureAllocated(var);

        if (const ExprNode* valueExpr = node.value())
        {
            valueExpr->accept(*this);
            CodegenValue value = popValue();
            value = ensureType(std::move(value), var.type);
            storeValue(var, value);
        }
    }

    void visitIf(const IfNode& node) override
    {
        if (!node.condition() || !node.thenBlock())
            return;

        node.condition()->accept(*this);
        CodegenValue condValue = popValue();
        condValue = ensureType(std::move(condValue), ValueType::Bool);

        string thenLabel = nextLabel("then");
        string endLabel = nextLabel("endif");
        bool hasElse = node.elseBlock() != nullptr;
        string elseLabel = hasElse ? nextLabel("else") : "";

        string falseLabel = hasElse ? elseLabel : endLabel;

        emitInstruction("br i1 " + condValue.operand + ", label %" + thenLabel + ", label %" + falseLabel);
        m_currentBlockTerminated = true;

        emitLabel(thenLabel);
        bool thenFallsThrough = generateBlock(*node.thenBlock(), endLabel);

        bool elseFallsThrough = false;
        if (hasElse)
        {
            emitLabel(elseLabel);
            elseFallsThrough = generateBlock(*node.elseBlock(), endLabel);
        }

        emitLabel(endLabel);

        if (hasElse && !thenFallsThrough && !elseFallsThrough)
            m_currentBlockTerminated = true;
        else
            m_currentBlockTerminated = false;
    }

    void visitReturn(const ReturnNode& node) override
    {
        if (!node.expr())
            return;

        node.expr()->accept(*this);
        CodegenValue value = popValue();
        emitReturn(std::move(value));
    }

    void visitBinaryOp(const BinaryOpNode& node) override
    {
        if (const ExprNode* left = node.left())
            left->accept(*this);
        if (const ExprNode* right = node.right())
            right->accept(*this);

        CodegenValue rightValue = popValue();
        CodegenValue leftValue = popValue();

        switch (node.op())
        {
        case BinaryOpNode::Operator::Add:
        case BinaryOpNode::Operator::Sub:
        case BinaryOpNode::Operator::Mul:
        {
            ValueType targetType = node.type();
            leftValue = ensureType(std::move(leftValue), targetType);
            rightValue = ensureType(std::move(rightValue), targetType);

            const char* opInstr = (node.op() == BinaryOpNode::Operator::Add) ? "add" :
                (node.op() == BinaryOpNode::Operator::Sub) ? "sub" : "mul";
            string tmp = nextTemp();
            emitInstruction(tmp + " = " + opInstr + " " + llvmType(targetType) + " " +
                leftValue.operand + ", " + rightValue.operand);
            pushValue({ tmp, targetType });
            return;
        }
        case BinaryOpNode::Operator::Equal:
        case BinaryOpNode::Operator::NotEqual:
        {
            ValueType operandType = comparisonOperandType(leftValue.type, rightValue.type);
            leftValue = ensureType(std::move(leftValue), operandType);
            rightValue = ensureType(std::move(rightValue), operandType);

            const char* cmp = (node.op() == BinaryOpNode::Operator::Equal) ? "icmp eq" : "icmp ne";
            string tmp = nextTemp();
            emitInstruction(tmp + " = " + cmp + " " + llvmType(operandType) + " " +
                leftValue.operand + ", " + rightValue.operand);
            pushValue({ tmp, ValueType::Bool });
            return;
        }
        }

        pushValue({ zeroLiteral(ValueType::Invalid), ValueType::Invalid });
    }

    void visitUnaryOp(const UnaryOpNode& node) override
    {
        if (const ExprNode* operand = node.operand())
            operand->accept(*this);

        CodegenValue value = popValue();
        value = ensureType(std::move(value), ValueType::Bool);
        string tmp = nextTemp();
        emitInstruction(tmp + " = xor i1 " + value.operand + ", 1");
        pushValue({ tmp, ValueType::Bool });
    }

    void visitID(const IDNode& node) override
    {
        SymbolID symbolId = node.symbolId();
        if (symbolId == InvalidSymbolID)
        {
            pushValue({ "0", ValueType::Invalid });
            return;
        }

        CodegenVariable& var = getVariable(symbolId);
        ensureAllocated(var);
        if (!var.initialized)
            storeValue(var, { zeroLiteral(var.type), var.type });

        string tmp = nextTemp();
        emitInstruction(tmp + " = load " + llvmType(var.type) + ", " +
            llvmType(var.type) + "* " + var.pointer);
        pushValue({ tmp, var.type });
    }

    void visitNumber(const NumberNode& node) override
    {
        std::int64_t value = node.value();
        CodegenValue out;
        if (value >= std::numeric_limits<std::int32_t>::min() && value <= std::numeric_limits<std::int32_t>::max())
        {
            out.type = ValueType::I32;
            out.operand = std::to_string(static_cast<std::int32_t>(value));
        }
        else
        {
            out.type = ValueType::I64;
            out.operand = std::to_string(value);
        }
        pushValue(std::move(out));
    }

    void visitBoolLiteral(const BoolLiteralNode& node) override
    {
        pushValue({ node.value() ? "1" : "0", ValueType::Bool });
    }

private:
    string llvmType(ValueType type) const
    {
        switch (type)
        {
        case ValueType::I32: return "i32";
        case ValueType::I64: return "i64";
        case ValueType::Bool: return "i1";
        default: return "i32";
        }
    }

    string zeroLiteral(ValueType type) const
    {
        switch (type)
        {
        case ValueType::I32:
        case ValueType::I64:
        case ValueType::Bool:
            return "0";
        default:
            return "0";
        }
    }

    string nextTemp()
    {
        return "%t" + std::to_string(m_ctx.tempId++);
    }

    string nextLabel(const string& base)
    {
        return base + std::to_string(m_labelId++);
    }

    void emitLabel(const string& label)
    {
        m_ctx.ir << label << ":\n";
    }

    void emitInstruction(const string& text)
    {
        m_ctx.ir << "  " << text << "\n";
    }

    void pushValue(CodegenValue value)
    {
        m_stack.push_back(std::move(value));
    }

    CodegenValue popValue()
    {
        if (m_stack.empty())
            return { "0", ValueType::Invalid };
        CodegenValue value = std::move(m_stack.back());
        m_stack.pop_back();
        return value;
    }

    CodegenVariable& getVariable(SymbolID id)
    {
        auto it = m_variables.find(id);
        if (it == m_variables.end())
            it = m_variables.emplace(id, CodegenVariable{}).first;
        CodegenVariable& var = it->second;
        if (var.pointer.empty())
            var.pointer = "%tmpvar." + std::to_string(id);
        return var;
    }

    void ensureAllocated(CodegenVariable& var)
    {
        if (!var.allocated && !var.pointer.empty())
        {
            emitInstruction(var.pointer + " = alloca " + llvmType(var.type));
            var.allocated = true;
        }
    }

    CodegenValue ensureType(CodegenValue value, ValueType target)
    {
        if (target == ValueType::Invalid || value.type == ValueType::Invalid)
            return { value.operand, ValueType::Invalid };

        if (value.type == target)
            return value;

        if (target == ValueType::I64 && value.type == ValueType::I32)
        {
            string tmp = nextTemp();
            emitInstruction(tmp + " = sext i32 " + value.operand + " to i64");
            return { tmp, ValueType::I64 };
        }

        if (target == ValueType::I32 && value.type == ValueType::I64)
        {
            string tmp = nextTemp();
            emitInstruction(tmp + " = trunc i64 " + value.operand + " to i32");
            return { tmp, ValueType::I32 };
        }

        if (target == ValueType::I32 && value.type == ValueType::Bool)
        {
            string tmp = nextTemp();
            emitInstruction(tmp + " = zext i1 " + value.operand + " to i32");
            return { tmp, ValueType::I32 };
        }

        if (target == ValueType::I64 && value.type == ValueType::Bool)
        {
            CodegenValue widened = ensureType(std::move(value), ValueType::I32);
            return ensureType(std::move(widened), ValueType::I64);
        }

        if (target == ValueType::Bool && value.type != ValueType::Bool)
            return { value.operand, ValueType::Invalid };

        return value;
    }

    void storeValue(CodegenVariable& var, const CodegenValue& value)
    {
        ensureAllocated(var);
        emitInstruction("store " + llvmType(var.type) + " " + value.operand + ", " +
            llvmType(var.type) + "* " + var.pointer);
        var.initialized = true;
    }

    void emitReturn(CodegenValue value)
    {
        value = ensureType(std::move(value), ValueType::I32);
        string fmtPtr = nextTemp();
        emitInstruction(fmtPtr + " = getelementptr [29 x i8], [29 x i8]* @fmt, i32 0, i32 0");
        emitInstruction("call i32 (i8*, ...) @printf(i8* " + fmtPtr + ", i32 " + value.operand + ")");
        emitInstruction("ret i32 " + value.operand);
        m_currentBlockTerminated = true;
    }

    bool generateBlock(const BlockNode& node, const string& exitLabel)
    {
        bool savedTerminated = m_currentBlockTerminated;
        m_currentBlockTerminated = false;

        for (const auto& stmt : node.statements())
        {
            if (m_currentBlockTerminated)
                break;
            if (stmt)
                stmt->accept(*this);
        }

        bool fallsThrough = !m_currentBlockTerminated;
        if (fallsThrough && !exitLabel.empty())
            emitInstruction("br label %" + exitLabel);

        m_currentBlockTerminated = savedTerminated;
        return fallsThrough;
    }

    IRContext& m_ctx;
    std::unordered_map<SymbolID, CodegenVariable> m_variables;
    std::vector<CodegenValue> m_stack;
    int m_labelId = 0;
    bool m_currentBlockTerminated = false;
};

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: compiler <source> <output>" << std::endl;
        return 1;
    }

    std::ifstream fin(argv[1]);
    if (!fin)
    {
        std::cerr << "Cannot open file" << std::endl;
        return 1;
    }

    std::ostringstream buffer;
    buffer << fin.rdbuf();
    string source = buffer.str();
    fin.close();

    auto tokens = lexSource(source);
    SyntaxParser parser(tokens);
    auto program = parser.parseProgram();

    if (!program || parser.hasErrors())
    {
        const auto& errs = parser.errors();
        if (errs.empty())
        {
            std::cerr << "Parse error: unable to build AST" << std::endl;
        }
        else
        {
            for (const auto& err : errs)
                std::cerr << "Parse error: " << err << std::endl;
        }
        return 1;
    }

    SemanticAnalyzer semantic;
    bool semanticOk = semantic.analyze(*program);

    for (const auto& warning : semantic.warnings())
        std::cerr << "Warning: " << warning << std::endl;

    if (!semanticOk)
    {
        for (const auto& err : semantic.errors())
            std::cerr << "Semantic error: " << err << std::endl;
        return 1;
    }

    IRContext ctx;
    ctx.ir << "declare i32 @printf(i8*, ...)\n\n";
    ctx.ir << "@fmt = private constant [29 x i8] c\"Program exit with result %d\\0A\\00\"\n\n";
    ctx.ir << "define i32 @main() {\n";

    CodeGenerator generator(ctx, semantic.symbols());
    generator.generate(*program);

    ctx.ir << "}\n";

    string filename;
    if (argc >= 3)
    {
        filename = argv[2];
    }
    else
    {
        filename = argv[1];
        size_t dot = filename.find_last_of('.');
        if (dot != string::npos)
            filename = filename.substr(0, dot);
        filename += ".ll";
    }

    std::ofstream fout(filename);
    fout << ctx.ir.str();
    fout.close();

    return 0;
}
