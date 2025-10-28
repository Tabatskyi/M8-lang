#include "CodeGen.hpp"

CodeGenerator::CodeGenerator(IRContext& ctx, const std::unordered_map<SymbolID, VariableInfo>& symbols) : m_ctx(ctx)
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

void CodeGenerator::generate(const ProgramNode& program)
{
    m_currentBlockTerminated = false;
    program.accept(*this);
    if (!m_currentBlockTerminated)
        emitReturn({ "0", ValueType::I32 });
}

void CodeGenerator::visitProgram(const ProgramNode& node) 
{
    for (const auto& stmt : node.statements())
    {
        if (m_currentBlockTerminated)
            break;
        if (stmt)
            stmt->accept(*this);
    }
}

void CodeGenerator::visitBlock(const BlockNode& node) 
{
    generateBlock(node, "");
}

void CodeGenerator::visitDecl(const DeclNode& node) 
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

void CodeGenerator::visitAssign(const AssignNode& node) 
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

void CodeGenerator::visitIf(const IfNode& node) 
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

void CodeGenerator::visitReturn(const ReturnNode& node) 
{
    if (!node.expr())
    {
        emitReturn({ "0", ValueType::I32 });
        return;
    }

    node.expr()->accept(*this);
    CodegenValue value = popValue();
    emitReturn(std::move(value));
}

void CodeGenerator::visitBinaryOp(const BinaryOpNode& node) 
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
    case BinaryOpNode::Operator::Div:
    {
        ValueType targetType = node.type();
        leftValue = ensureType(std::move(leftValue), targetType);
        rightValue = ensureType(std::move(rightValue), targetType);

        string opInstr;
        if (node.op() == BinaryOpNode::Operator::Add)
            opInstr = "add";
        else if (node.op() == BinaryOpNode::Operator::Sub)
            opInstr = "sub";
        else if (node.op() == BinaryOpNode::Operator::Mul)
            opInstr = "mul";
        else if (node.op() == BinaryOpNode::Operator::Div)
            opInstr = "sdiv";

        string tmp = nextTemp();
        emitInstruction(tmp + " = " + opInstr + " " + llvmType(targetType) + " " + leftValue.operand + ", " + rightValue.operand);
        pushValue({ tmp, targetType });
        return;
    }
    case BinaryOpNode::Operator::Equal:
    case BinaryOpNode::Operator::NotEqual:
    {
        ValueType operandType = comparisonOperandType(leftValue.type, rightValue.type);
        leftValue = ensureType(std::move(leftValue), operandType);
        rightValue = ensureType(std::move(rightValue), operandType);

        string cmp = (node.op() == BinaryOpNode::Operator::Equal) ? "icmp eq" : "icmp ne";
        string tmp = nextTemp();
        emitInstruction(tmp + " = " + cmp + " " + llvmType(operandType) + " " + leftValue.operand + ", " + rightValue.operand);
        pushValue({ tmp, ValueType::Bool });
        return;
    }
    }

    pushValue({ zeroLiteral(ValueType::Invalid), ValueType::Invalid });
}

void CodeGenerator::visitUnaryOp(const UnaryOpNode& node) 
{
    if (const ExprNode* operand = node.operand())
        operand->accept(*this);

    CodegenValue value = popValue();
    value = ensureType(std::move(value), ValueType::Bool);
    string tmp = nextTemp();
    emitInstruction(tmp + " = xor i1 " + value.operand + ",1");
    pushValue({ tmp, ValueType::Bool });
}

void CodeGenerator::visitID(const IDNode& node) 
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
    emitInstruction(tmp + " = load " + llvmType(var.type) + ", " + llvmType(var.type) + "* " + var.pointer);
    pushValue({ tmp, var.type });
}

void CodeGenerator::visitNumber(const NumberNode& node) 
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

void CodeGenerator::visitBoolLiteral(const BoolLiteralNode& node) 
{
    pushValue({ node.value() ? "1" : "0", ValueType::Bool });
}

string CodeGenerator::llvmType(ValueType type) const
{
    switch (type)
    {
    case ValueType::I32: return "i32";
    case ValueType::I64: return "i64";
    case ValueType::Bool: return "i1";
    default: return "i32";
    }
}

string CodeGenerator::zeroLiteral(ValueType type) const
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

string CodeGenerator::nextTemp()
{
    return "%t" + std::to_string(m_ctx.tempId++);
}

string CodeGenerator::nextLabel(const string& base)
{
    return base + std::to_string(m_labelId++);
}

void CodeGenerator::emitLabel(const string& label)
{
    m_ctx.ir << label << ":\n";
}

void CodeGenerator::emitInstruction(const string& text)
{
    m_ctx.ir << " " << text << "\n";
}

void CodeGenerator::pushValue(CodegenValue value)
{
    m_stack.push_back(std::move(value));
}

CodeGenerator::CodegenValue CodeGenerator::popValue()
{
    if (m_stack.empty())
        return { "0", ValueType::Invalid };
    CodegenValue value = std::move(m_stack.back());
    m_stack.pop_back();
    return value;
}

CodeGenerator::CodegenVariable& CodeGenerator::getVariable(SymbolID id)
{
    auto it = m_variables.find(id);
    if (it == m_variables.end())
        it = m_variables.emplace(id, CodegenVariable{}).first;
    CodegenVariable& var = it->second;
    if (var.pointer.empty())
        var.pointer = "%tmpvar." + std::to_string(id);
    return var;
}

void CodeGenerator::ensureAllocated(CodegenVariable& var)
{
    if (!var.allocated && !var.pointer.empty())
    {
        emitInstruction(var.pointer + " = alloca " + llvmType(var.type));
        var.allocated = true;
    }
}

CodeGenerator::CodegenValue CodeGenerator::ensureType(CodeGenerator::CodegenValue value, ValueType target)
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

    if (target == ValueType::Bool)
    {
        if (value.type == ValueType::Bool)
            return value;
        if (value.type == ValueType::I32)
        {
            string tmp = nextTemp();
            emitInstruction(tmp + " = icmp ne i32 " + value.operand + ", 0");
            return { tmp, ValueType::Bool };
        }
        if (value.type == ValueType::I64)
        {
            string tmp = nextTemp();
            emitInstruction(tmp + " = icmp ne i64 " + value.operand + ", 0");
            return { tmp, ValueType::Bool };
        }
        return { value.operand, ValueType::Invalid };
    }

    return value;
}

void CodeGenerator::storeValue(CodeGenerator::CodegenVariable& var, const CodeGenerator::CodegenValue& value)
{
    ensureAllocated(var);
    emitInstruction("store " + llvmType(var.type) + " " + value.operand + ", " + llvmType(var.type) + "* " + var.pointer);
    var.initialized = true;
}

void CodeGenerator::emitReturn(CodeGenerator::CodegenValue value)
{
    value = ensureType(std::move(value), ValueType::I32);
    string fmtPtr = nextTemp();
    emitInstruction(fmtPtr + " = getelementptr [29 x i8], [29 x i8]* @fmt, i32 0, i32 0");
    emitInstruction("call i32 (i8*, ...) @printf(i8* " + fmtPtr + ", i32 " + value.operand + ")");
    emitInstruction("ret i32 " + value.operand);
    m_currentBlockTerminated = true;
}

bool CodeGenerator::generateBlock(const BlockNode& node, const string& exitLabel)
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