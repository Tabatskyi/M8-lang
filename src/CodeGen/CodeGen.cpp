#include "CodeGen.hpp"
#include "../General/Utility.hpp"

#include "../AST/ProgramNode.hpp"
#include "../AST/BlockNode.hpp"
#include "../AST/DeclNode.hpp"
#include "../AST/AssignNode.hpp"
#include "../AST/AssignFieldNode.hpp"
#include "../AST/FieldAccessNode.hpp"
#include "../AST/StructDecNode.hpp"
#include "../AST/FunctionNode.hpp" 
#include "../AST/FunctionCallNode.hpp" 
#include "../AST/MemberFunctionCallNode.hpp"
#include "../AST/IfNode.hpp"
#include "../AST/ReturnNode.hpp"
#include "../AST/BinaryOpNode.hpp"
#include "../AST/UnaryOpNode.hpp"
#include "../AST/IDNode.hpp"
#include "../AST/NumberNode.hpp"
#include "../AST/BoolLiteralNode.hpp"

CodeGenerator::CodeGenerator(IRContext& ctx, const std::unordered_map<SymbolID, VariableInfo>& symbols) : _ctx(ctx)
{
    for (const auto& [id, info] : symbols)
    {
        CodegenVariable var;
        var.type = info.type;
        var.isMutable = info.isMutable;
        var.pointer = "%" + info.name + "." + std::to_string(id);
        _variables.emplace(id, std::move(var));
    }
}

void CodeGenerator::scanFunctions(const ProgramNode& program)
{
    for (const auto& stmt : program.statements())
    {
        if (!stmt) continue;
        if (auto* fn = dynamic_cast<FunctionNode*>(stmt.get()))
        {
            FunctionSignature sig;
            if (fn->returnType().kind == TypeDesc::Kind::Builtin)
                sig.returnType = fn->returnType().builtin;
            for (const auto& p : fn->params())
            {
                if (p.type.kind == TypeDesc::Kind::Builtin)
                    sig.paramTypes.push_back(p.type.builtin);
                else
                    sig.paramTypes.push_back(ValueType::Invalid);
            }
            _functions.emplace(fn->name(), sig);
        }
        else if (auto* sd = dynamic_cast<StructDeclNode*>(stmt.get()))
        {
            for (const auto& mptr : sd->functions())
            {
                if (!mptr) continue;
                FunctionSignature sig;
                if (mptr->returnType().kind == TypeDesc::Kind::Builtin)
                    sig.returnType = mptr->returnType().builtin;
                for (const auto& p : mptr->params())
                {
                    if (p.type.kind == TypeDesc::Kind::Builtin)
                        sig.paramTypes.push_back(p.type.builtin);
                    else
                        sig.paramTypes.push_back(ValueType::Invalid);
                }
                _functions.emplace(mptr->name(), sig);
            }
        }
    }
}

void CodeGenerator::generate(const ProgramNode& program)
{
    scanFunctions(program);

    for (const auto& stmt : program.statements())
    {
        if (!stmt) continue;
        if (auto* fn = dynamic_cast<FunctionNode*>(stmt.get()))
        {
            if (fn->name() == "main") 
                continue;
            fn->accept(*this);
        }
        else if (auto* sd = dynamic_cast<StructDeclNode*>(stmt.get()))
        {
            for (const auto& mptr : sd->functions())
            {
                if (mptr && mptr->name() != "main")
                    mptr->accept(*this);
            }
        }
    }

    bool mainFound = _functions.count("main") > 0;
    if (mainFound)
    {
        for (const auto& stmt : program.statements())
        {
            if (auto* fn = dynamic_cast<FunctionNode*>(stmt.get()); fn && fn->name() == "main")
            {
                fn->accept(*this);
                break;
            }
        }
    }
    else
    {
        _ctx.ir << "define i32 @main() {\n";
        emitInstruction("ret i32 0");
        _ctx.ir << "}\n";
    }
}

void CodeGenerator::visitProgram(const ProgramNode& node) {}

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
    if (!node.initializers().empty() && node.initializers().front())
    {
        const ExprNode* init = node.initializers().front().get();
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

void CodeGenerator::visitAssignField(const AssignFieldNode& node)
{
    if (node.value())
        node.value()->accept(*this);

    if (node.value())
        popValue();
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
    _currentBlockTerminated = true;

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
        _currentBlockTerminated = true;
    else
        _currentBlockTerminated = false;
}

void CodeGenerator::visitReturn(const ReturnNode& node) 
{
    CodegenValue value{ zeroLiteral(_currentFunctionReturnType), _currentFunctionReturnType };
    if (node.expr())
    {
        node.expr()->accept(*this);
        value = popValue();
        value = ensureType(std::move(value), _currentFunctionReturnType);
    }
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

void CodeGenerator::visitStructDecl(const StructDeclNode& node)
{
    for (const auto& func : node.functions())
    {
        if (func)
            func->accept(*this);
    }
}

void CodeGenerator::visitFunction(const FunctionNode& node)
{
    FunctionSignature sig = _functions[node.name()];
    _currentFunctionReturnType = sig.returnType == ValueType::Invalid ? ValueType::I32 : sig.returnType;
    _insideFunction = true;

    std::string paramList;
    for (size_t i = 0; i < sig.paramTypes.size(); ++i)
    {
        if (i) paramList += ", ";
        paramList += llvmType(sig.paramTypes[i]);
    }

    _ctx.ir << "define " << llvmType(_currentFunctionReturnType) << " @" << node.name() << "(" << paramList << ") {\n";

    size_t idx = 0;
    for (const auto& p : node.params())
    {
        SymbolID sid = p.symbolId;
        CodegenVariable& var = getVariable(sid);
        std::string rawArg = "%" + std::to_string(idx++);
        var.pointer = "%" + p.name + "." + std::to_string(sid);
        emitInstruction(var.pointer + " = alloca " + llvmType(sig.paramTypes[idx-1]));
        emitInstruction("store " + llvmType(sig.paramTypes[idx-1]) + " " + rawArg + ", " + llvmType(sig.paramTypes[idx-1]) + "* " + var.pointer);
        var.type = sig.paramTypes[idx-1];
        var.initialized = true;
        var.allocated = true;
    }

    if (const BlockNode* body = node.body())
        body->accept(*this);

    if (!_currentBlockTerminated)
    {
        emitInstruction("ret " + llvmType(_currentFunctionReturnType) + " " + zeroLiteral(_currentFunctionReturnType));
    }

    _ctx.ir << "}\n";

    _insideFunction = false;
    _currentBlockTerminated = false;
}

void CodeGenerator::visitFieldAccess(const FieldAccessNode& node)
{
    pushValue({ "0", ValueType::Invalid });
}

void CodeGenerator::visitFunctionCall(const FunctionCallNode& node)
{
    if (node.name() == "__builtin_write")
    {
        if (!node.args().empty() && node.args().front())
        {
            node.args().front()->accept(*this);
            CodegenValue value = popValue();
            value = ensureType(std::move(value), ValueType::I32);
            std::string fmtPtr = nextTemp();
            emitInstruction(fmtPtr + " = getelementptr [4 x i8], [4 x i8]* @fmtw, i32 0, i32 0");
            std::string tmp = nextTemp();
            emitInstruction(tmp + " = call i32 (i8*, ...) @printf(i8* " + fmtPtr + ", i32 " + value.operand + ")");
            pushValue({ tmp, ValueType::I32 });
        }
        else
        {
            pushValue({ "0", ValueType::I32 });
        }
        return;
    }
    if (node.name() == "__builtin_read")
    {
        std::string ptr = nextTemp();
        emitInstruction(ptr + " = alloca i32");
        std::string fmtPtr = nextTemp();
        emitInstruction(fmtPtr + " = getelementptr [3 x i8], [3 x i8]* @fmtr, i32 0, i32 0");
        emitInstruction("call i32 (i8*, ...) @scanf(i8* " + fmtPtr + ", i32* " + ptr + ")");
        std::string loadTmp = nextTemp();
        emitInstruction(loadTmp + " = load i32, i32* " + ptr);
        pushValue({ loadTmp, ValueType::I32 });
        return;
    }

    auto it = _functions.find(node.name());
    if (it == _functions.end())
    {
        for (const auto& arg : node.args())
        {
            if (arg) { arg->accept(*this); popValue(); }
        }
        pushValue({ "0", ValueType::I32 });
        return;
    }

    const FunctionSignature& sig = it->second;
    std::vector<CodegenValue> argValues;
    for (size_t i = 0; i < node.args().size(); ++i)
    {
        if (node.args()[i])
        {
            node.args()[i]->accept(*this);
            CodegenValue v = popValue();
            ValueType expected = (i < sig.paramTypes.size()) ? sig.paramTypes[i] : ValueType::Invalid;
            v = ensureType(std::move(v), expected);
            argValues.push_back(v);
        }
    }

    std::string callTmp = nextTemp();
    std::string irCall = callTmp + " = call " + llvmType(sig.returnType == ValueType::Invalid ? ValueType::I32 : sig.returnType) + " @" + node.name() + "(";
    for (size_t i = 0; i < argValues.size(); ++i)
    {
        if (i) irCall += ", ";
        ValueType t = (i < sig.paramTypes.size()) ? sig.paramTypes[i] : argValues[i].type;
        irCall += llvmType(t) + " " + argValues[i].operand;
    }
    irCall += ")";
    emitInstruction(irCall);
    pushValue({ callTmp, sig.returnType == ValueType::Invalid ? ValueType::I32 : sig.returnType });
}

void CodeGenerator::visitMemberFunctionCall(const MemberFunctionCallNode& node)
{
    auto it = _functions.find(node.funcName());
    std::vector<CodegenValue> argValues;
    for (size_t i = 0; i < node.args().size(); ++i)
    {
        if (node.args()[i])
        {
            node.args()[i]->accept(*this);
            CodegenValue v = popValue();
            ValueType expected = ValueType::Invalid;
            if (it != _functions.end() && i < it->second.paramTypes.size())
                expected = it->second.paramTypes[i];
            v = ensureType(std::move(v), expected);
            argValues.push_back(v);
        }
    }
    if (it == _functions.end())
    {
        pushValue({ "0", ValueType::I32 });
        return;
    }
    const FunctionSignature& sig = it->second;
    std::string callTmp = nextTemp();
    std::string irCall = callTmp + " = call " + llvmType(sig.returnType == ValueType::Invalid ? ValueType::I32 : sig.returnType) + " @" + node.funcName() + "(";
    for (size_t i = 0; i < argValues.size(); ++i)
    {
        if (i) irCall += ", ";
        ValueType t = (i < sig.paramTypes.size()) ? sig.paramTypes[i] : argValues[i].type;
        irCall += llvmType(t) + " " + argValues[i].operand;
    }
    irCall += ")";
    emitInstruction(irCall);
    pushValue({ callTmp, sig.returnType == ValueType::Invalid ? ValueType::I32 : sig.returnType });
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
    return "%t" + std::to_string(_ctx.tempId++);
}

string CodeGenerator::nextLabel(const string& base)
{
    return base + std::to_string(_labelId++);
}

void CodeGenerator::emitLabel(const string& label)
{
    _ctx.ir << label << ":\n";
}

void CodeGenerator::emitInstruction(const string& text)
{
    _ctx.ir << " " << text << "\n";
}

void CodeGenerator::pushValue(CodegenValue value)
{
    _stack.push_back(std::move(value));
}

CodegenValue CodeGenerator::popValue()
{
    if (_stack.empty())
        return { "0", ValueType::Invalid };
    CodegenValue value = std::move(_stack.back());
    _stack.pop_back();
    return value;
}

CodegenVariable& CodeGenerator::getVariable(SymbolID id)
{
    auto it = _variables.find(id);
    if (it == _variables.end())
        it = _variables.emplace(id, CodegenVariable{}).first;
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

CodegenValue CodeGenerator::ensureType(CodegenValue value, ValueType target)
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

void CodeGenerator::storeValue(CodegenVariable& var, const CodegenValue& value)
{
    ensureAllocated(var);
    emitInstruction("store " + llvmType(var.type) + " " + value.operand + ", " + llvmType(var.type) + "* " + var.pointer);
    var.initialized = true;
}

void CodeGenerator::emitReturn(CodegenValue value)
{
    value = ensureType(std::move(value), _currentFunctionReturnType);
    emitInstruction("ret " + llvmType(_currentFunctionReturnType) + " " + value.operand);
    _currentBlockTerminated = true;
}

bool CodeGenerator::generateBlock(const BlockNode& node, const string& exitLabel)
{
    bool savedTerminated = _currentBlockTerminated;
    _currentBlockTerminated = false;

    for (const auto& stmt : node.statements())
    {
        if (_currentBlockTerminated)
            break;
        if (stmt)
            stmt->accept(*this);
    }

    bool fallsThrough = !_currentBlockTerminated;
    if (fallsThrough && !exitLabel.empty())
        emitInstruction("br label %" + exitLabel);

    _currentBlockTerminated = savedTerminated;
    return fallsThrough;
}