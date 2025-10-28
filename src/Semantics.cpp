#include "Semantics.hpp"

#include <limits>
#include <sstream>

bool SemanticAnalyzer::analyze(const ProgramNode& program)
{
    m_errors.clear();
    m_warnings.clear();
    m_symbols.clear();
    m_scopeSymbols.clear();
    m_scopeStack.clear();
    m_nextSymbolId =0;
    m_returnSeen = false;

    program.accept(*this);

    return m_errors.empty();
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
        if (node.declaredType() != ValueType::Invalid)
        {
            if (!isAssignable(node.declaredType(), initType))
            {
                addError("Cannot initialize '" + name + "' of type " + typeToString(node.declaredType()) + " with value of type " + typeToString(initType));
            }
        }
    }

    SymbolID symbolId = m_nextSymbolId++;
    scopeMap.emplace(name, symbolId);
    ValueType varType = node.declaredType();
    if (varType == ValueType::Invalid && node.initializer())
        varType = node.initializer()->type();
    m_symbols.emplace(symbolId, VariableInfo{ varType, node.isMutable(), name, scope });
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
        const auto& info = m_symbols[symbolId];
        if (!info.isMutable)
            addError("Variable '" + node.identifier() + "' is immutable");
        const_cast<AssignNode&>(node).setSymbolId(symbolId);
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
                addError("Cannot assign value of type " + typeToString(valueType) + " to variable '" + node.identifier() + "' of type " + typeToString(info.type));
            }
        }
    }
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
    m_returnSeen = true;
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
    const_cast<IDNode&>(node).setType(m_symbols.at(symbolId).type);
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

void SemanticAnalyzer::enterScope(size_t scopeId)
{
    m_scopeStack.push_back(scopeId);
    m_scopeSymbols.try_emplace(scopeId, std::unordered_map<std::string, SymbolID>{});
}

void SemanticAnalyzer::exitScope()
{
    if (!m_scopeStack.empty())
        m_scopeStack.pop_back();
}

size_t SemanticAnalyzer::currentScopeId() const
{
    return m_scopeStack.empty() ?0 : m_scopeStack.back();
}

SymbolID SemanticAnalyzer::resolveSymbol(const std::string& name) const
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

void SemanticAnalyzer::addError(const std::string& message)
{
    m_errors.push_back(message);
}

void SemanticAnalyzer::addWarning(const std::string& message)
{
    m_warnings.push_back(message);
}