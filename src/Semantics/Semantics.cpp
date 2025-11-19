#include "Semantics.hpp"
#include "../General/Utility.hpp"
#include <limits>
#include <sstream>

#include "../AST/ProgramNode.hpp"
#include "../AST/BlockNode.hpp"
#include "../AST/DeclNode.hpp"
#include "../AST/AssignNode.hpp"
#include "../AST/IfNode.hpp"
#include "../AST/ReturnNode.hpp"
#include "../AST/BinaryOpNode.hpp"
#include "../AST/UnaryOpNode.hpp"
#include "../AST/IDNode.hpp"
#include "../AST/NumberNode.hpp"
#include "../AST/BoolLiteralNode.hpp"
#include "../AST/AssignFieldNode.hpp"
#include "../AST/FieldAccessNode.hpp"
#include "../AST/StructDecNode.hpp"
#include "../AST/FunctionNode.hpp"
#include "../AST/FunctionCallNode.hpp"
#include "../AST/MemberFunctionCallNode.hpp"

bool SemanticAnalyzer::analyze(const ProgramNode& program)
{
    _errors.clear();
    _warnings.clear();
    _symbols.clear();
    _scopeSymbols.clear();
    _scopeStack.clear();
    _nextSymbolId = 0;
    _returnSeen = false;

    program.accept(*this);

    return _errors.empty();
}

void SemanticAnalyzer::visitProgram(const ProgramNode& node)
{
    enterScope(node.scopeId());
    for (const auto& stmt : node.statements())
    {
        if (stmt)
            stmt->accept(*this);
    }
    exitScope();
}

void SemanticAnalyzer::visitBlock(const BlockNode& node)
{
    enterScope(node.scopeId());
    for (const auto& stmt : node.statements())
    {
        if (stmt)
            stmt->accept(*this);
    }
    exitScope();
}

void SemanticAnalyzer::visitDecl(const DeclNode& node)
{
    const std::string& name = node.identifier();
    size_t scope = currentScopeId();
    auto& scopeMap = _scopeSymbols[scope];
    if (scopeMap.count(name))
    {
        addError("Variable '" + name + "' redeclared");
        return;
    }

    for (const auto& initPtr : node.initializers())
    {
        if (initPtr)
        {
            initPtr->accept(*this);
            ValueType initType = initPtr->type();
            if (node.declaredType().kind == TypeDesc::Kind::Builtin && node.declaredType().builtin != ValueType::Invalid)
            {
                if (!isAssignable(node.declaredType().builtin, initType))
                {
                    addError("Cannot initialize '" + name + "' of type " + typeToString(node.declaredType().builtin) + " with value of type " + typeToString(initType));
                }
            }
        }
    }

    SymbolID symbolId = _nextSymbolId++;
    scopeMap.emplace(name, symbolId);
    ValueType varType = ValueType::Invalid;
    if (node.declaredType().kind == TypeDesc::Kind::Builtin)
        varType = node.declaredType().builtin;
    if (varType == ValueType::Invalid && !node.initializers().empty() && node.initializers().front())
        varType = node.initializers().front()->type();
    _symbols.emplace(symbolId, VariableInfo{ varType, node.isMutable(), name, scope });
    node.setSymbolId(symbolId);
}

void SemanticAnalyzer::visitAssign(const AssignNode& node)
{
    SymbolID symbolId = resolveSymbol(node.identifier());
    if (symbolId == InvalidSymbolID)
    {
        addError("Assignment to undeclared variable '" + node.identifier() + "'");
    }
    else
    {
        const auto& info = _symbols[symbolId];
        if (!info.isMutable)
            addError("Variable '" + node.identifier() + "' is immutable");
        const_cast<AssignNode&>(node).setSymbolId(symbolId);
    }

    if (const ExprNode* value = node.value())
    {
        value->accept(*this);
        if (symbolId != InvalidSymbolID)
        {
            const auto& info = _symbols[symbolId];
            ValueType valueType = value->type();
            if (!isAssignable(info.type, valueType))
            {
                addError("Cannot assign value of type " + typeToString(valueType) + " to variable '" + node.identifier() + "' of type " + typeToString(info.type));
            }
        }
    }
}

void SemanticAnalyzer::visitAssignField(const AssignFieldNode& node)
{
    if (const FieldAccessNode* target = node.target())
        target->accept(*this);
    if (const ExprNode* value = node.value())
        value->accept(*this);
}

void SemanticAnalyzer::visitIf(const IfNode& node)
{
    if (const ExprNode* cond = node.condition())
    {
        cond->accept(*this);
        ValueType condType = cond->type();
        if (condType != ValueType::Bool && !isNumeric(condType))
            addError("Condition of if statement must be bool or integer (0=false, nonzero=true)");
    }

    if (const BlockNode* thenBlock = node.thenBlock())
        thenBlock->accept(*this);
    if (const BlockNode* elseBlock = node.elseBlock())
        elseBlock->accept(*this);
}

void SemanticAnalyzer::visitReturn(const ReturnNode& node)
{
    _returnSeen = true;
    if (!node.expr())
        return;

    node.expr()->accept(*this);
    ValueType type = node.expr()->type();
    if (!canConvertToI32(type))
        addError("Return type must be convertible to i32, got " + typeToString(type));
}

void SemanticAnalyzer::visitBinaryOp(const BinaryOpNode& node)
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
    case BinaryOpNode::Operator::Div:
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

void SemanticAnalyzer::visitUnaryOp(const UnaryOpNode& node)
{
    if (const ExprNode* operand = node.operand())
        operand->accept(*this);

    const ExprNode* operand = node.operand();
    ValueType operandType = operand ? operand->type() : ValueType::Invalid;
    if (operandType != ValueType::Bool && !isNumeric(operandType))
    {
        addError("Logical not operator requires bool or integer operand");
        node.setType(ValueType::Invalid);
        return;
    }

    node.setType(ValueType::Bool);
}

void SemanticAnalyzer::visitID(const IDNode& node)
{
    SymbolID symbolId = resolveSymbol(node.name());
    if (symbolId == InvalidSymbolID)
    {
        addError("Use of undeclared variable '" + node.name() + "'");
        node.setType(ValueType::Invalid);
        return;
    }
    const_cast<IDNode&>(node).setSymbolId(symbolId);
    const_cast<IDNode&>(node).setType(_symbols.at(symbolId).type);
}

void SemanticAnalyzer::visitNumber(const NumberNode& node)
{
    std::int64_t value = node.value();
    if (value >= std::numeric_limits<std::int32_t>::min() && value <= std::numeric_limits<std::int32_t>::max())
        const_cast<NumberNode&>(node).setType(ValueType::I32);
    else
        const_cast<NumberNode&>(node).setType(ValueType::I64);
}

void SemanticAnalyzer::visitBoolLiteral(const BoolLiteralNode& node)
{
    const_cast<BoolLiteralNode&>(node).setType(ValueType::Bool);
}

void SemanticAnalyzer::visitStructDecl(const StructDeclNode& node)
{
    for (const auto& field : node.fields())
    {
        if (field.type.kind == TypeDesc::Kind::Builtin && field.type.builtin == ValueType::Invalid)
            addWarning("Field '" + field.name + "' has invalid type");
    }
    for (const auto& funcPtr : node.functions())
    {
        if (funcPtr)
            funcPtr->accept(*this);
    }
}

void SemanticAnalyzer::visitFunction(const FunctionNode& node)
{
    enterScope(node.scopeId());

    for (const auto& param : node.params())
    {
        SymbolID sid = _nextSymbolId++;
        _scopeSymbols[currentScopeId()][param.name] = sid;
        _symbols.emplace(sid, VariableInfo{ param.type.kind == TypeDesc::Kind::Builtin ? param.type.builtin : ValueType::Invalid, true, param.name, currentScopeId() });
        const_cast<FunctionNode::Param&>(param).symbolId = sid;
    }
    if (const BlockNode* body = node.body())
        body->accept(*this);
    exitScope();
    if (!_returnSeen && (node.returnType().kind == TypeDesc::Kind::Builtin && node.returnType().builtin != ValueType::Invalid))
        addWarning("Function '" + node.name() + "' may not return a value");
    _returnSeen = false; 
}

void SemanticAnalyzer::visitFieldAccess(const FieldAccessNode& node)
{
    SymbolID baseId = resolveSymbol(node.base());
    if (baseId == InvalidSymbolID)
    {
        addError("Field access of undeclared base '" + node.base() + "'");
        return;
    }
    const_cast<FieldAccessNode&>(node).setBaseSymbolId(baseId);
}

void SemanticAnalyzer::visitFunctionCall(const FunctionCallNode& node)
{
    for (const auto& arg : node.args())
    {
        if (arg)
            arg->accept(*this);
    }
}

void SemanticAnalyzer::visitMemberFunctionCall(const MemberFunctionCallNode& node)
{
    SymbolID baseId = resolveSymbol(node.base());
    if (baseId == InvalidSymbolID)
    {
        addError("Member function call on undeclared base '" + node.base() + "'");
    }
    else
    {
        const_cast<MemberFunctionCallNode&>(node).setBaseSymbolId(baseId);
    }
    for (const auto& arg : node.args())
    {
        if (arg)
            arg->accept(*this);
    }
}

void SemanticAnalyzer::enterScope(size_t scopeId)
{
    _scopeStack.push_back(scopeId);
    _scopeSymbols.try_emplace(scopeId, std::unordered_map<std::string, SymbolID>{});
}

void SemanticAnalyzer::exitScope()
{
    if (!_scopeStack.empty())
        _scopeStack.pop_back();
}

size_t SemanticAnalyzer::currentScopeId() const
{
    return _scopeStack.empty() ? 0 : _scopeStack.back();
}

SymbolID SemanticAnalyzer::resolveSymbol(const std::string& name) const
{
    for (auto it = _scopeStack.rbegin(); it != _scopeStack.rend(); ++it)
    {
        auto scopeIt = _scopeSymbols.find(*it);
        if (scopeIt == _scopeSymbols.end())
            continue;

        auto symIt = scopeIt->second.find(name);
        if (symIt != scopeIt->second.end())
            return symIt->second;
    }
    return InvalidSymbolID;
}

void SemanticAnalyzer::addError(const std::string& message)
{
    _errors.push_back(message);
}

void SemanticAnalyzer::addWarning(const std::string& message)
{
    _warnings.push_back(message);
}