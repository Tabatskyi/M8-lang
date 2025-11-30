#include "Semantics.hpp"

namespace
{
std::string formatMessage(const std::string& message, std::size_t line)
{
    if (!line)
        return message;

    std::ostringstream oss;
    oss << message << " (line " << line << ')';
    return oss.str();
}
}

bool SemanticAnalyzer::analyze(const ProgramNode& program)
{
    _errors.clear();
    _warnings.clear();
    _symbols.clear();
    _scopeSymbols.clear();
    _scopeStack.clear();
    _structs.clear();
    _functions.clear();
    _nextSymbolId = 0;
    _inFunction = false;
    _returnSeen = false;
    _currentFunctionReturn = TypeDesc::Builtin(ValueType::Invalid);
    _currentMemberMaster.clear();

    registerBuiltins();
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

void SemanticAnalyzer::visitStructDecl(const StructDeclNode& node)
{
    if (_structs.count(node.name()))
    {
        addError("Struct '" + node.name() + "' redeclared", node);
        return;
    }

    StructInfo info;
    info.name = node.name();
    for (const auto& field : node.fields())
        info.fields.push_back(StructFieldInfo{ field.type, field.isMutable, field.name });

    _structs.emplace(info.name, std::move(info));
    for (const auto& method : node.functions())
    {
        if (method)
            method->accept(*this);
    }
}

void SemanticAnalyzer::visitFunction(const FunctionNode& node)
{
    if (_functions.count(node.name()))
    {
        addError("Function '" + node.name() + "' redeclared", node);
        return;
    }

    FunctionInfo info;
    info.name = node.name();
    info.returnType = node.returnType();
    info.scopeId = node.scopeId();
    info.isMember = node.isMember();
    info.masterStruct = node.isMember() ? node.masterStruct() : std::string{};
    info.isBuiltin = false;
    for (const auto& param : node.params())
        info.params.push_back(FunctionParamInfo{ param.type, param.name, InvalidSymbolID });
    auto [funcIt, inserted] = _functions.emplace(info.name, std::move(info));
    FunctionInfo* registeredInfo = &funcIt->second;

    enterScope(node.scopeId());
    for (size_t idx = 0; idx < node.params().size(); ++idx)
    {
        const auto& param = node.params()[idx];
        auto& scopeMap = _scopeSymbols[currentScopeId()];
        if (scopeMap.count(param.name))
        {
            addError("Parameter '" + param.name + "' redeclared", node);
            continue;
        }
        SymbolID sid = _nextSymbolId++;
        scopeMap.emplace(param.name, sid);
        _symbols.emplace(sid, VariableInfo{ param.type, false, param.name, currentScopeId() });
        const_cast<FunctionNode::Param&>(param).symbolId = sid;
        if (registeredInfo && idx < registeredInfo->params.size())
            registeredInfo->params[idx].symbolId = sid;
    }

    bool previousInFunction = _inFunction;
    bool previousReturnSeen = _returnSeen;
    TypeDesc previousReturn = _currentFunctionReturn;
    std::string previousMaster = _currentMemberMaster;

    _inFunction = true;
    _returnSeen = false;
    _currentFunctionReturn = node.returnType();
    _currentMemberMaster = node.isMember() ? node.masterStruct() : std::string{};

    if (const BlockNode* body = node.body())
        body->accept(*this);

    if (!_returnSeen && node.returnType().kind == TypeDesc::Kind::Builtin && node.returnType().builtin != ValueType::Invalid)
        addWarning("Function '" + node.name() + "' may not return a value", node);

    _inFunction = previousInFunction;
    _returnSeen = previousReturnSeen;
    _currentFunctionReturn = previousReturn;
    _currentMemberMaster = std::move(previousMaster);
    exitScope();
}

void SemanticAnalyzer::visitDecl(const DeclNode& node)
{
    const std::string& name = node.identifier();
    auto& scopeMap = _scopeSymbols[currentScopeId()];
    if (scopeMap.count(name))
    {
        addError("Variable '" + name + "' redeclared", node);
        return;
    }

    const ExprNode* initializer = nullptr;
    if (node.hasInitializer() && !node.initializers().empty())
    {
        initializer = node.initializers().front().get();
        if (initializer)
            initializer->accept(*this);
    }

    TypeDesc resolvedType = node.declaredType();
    if (resolvedType.kind == TypeDesc::Kind::Builtin && resolvedType.builtin != ValueType::Invalid)
    {
        if (initializer && !isAssignable(resolvedType.builtin, initializer->type()))
            addError("Cannot initialize '" + name + "' with incompatible type", initializer);
    }
    else if (resolvedType.kind == TypeDesc::Kind::Struct)
    {
        if (!_structs.count(resolvedType.structName))
            addError("Unknown struct type '" + resolvedType.structName + "'", node);
        if (initializer)
        {
            std::string initStruct;
            if (!extractStructType(initializer, initStruct))
                addError("Struct initializer must be a struct value", initializer);
            else if (initStruct != resolvedType.structName)
                addError("Struct initializer type mismatch", initializer);
        }
    }
    else if (initializer)
    {
        resolvedType = TypeDesc::Builtin(initializer->type());
    }

    if (resolvedType.kind == TypeDesc::Kind::Builtin && resolvedType.builtin == ValueType::Invalid && initializer)
        resolvedType = TypeDesc::Builtin(initializer->type());

    SymbolID symbolId = _nextSymbolId++;
    scopeMap.emplace(name, symbolId);
    _symbols.emplace(symbolId, VariableInfo{ resolvedType, node.isMutable(), name, currentScopeId() });
    node.setSymbolId(symbolId);
}

void SemanticAnalyzer::visitAssign(const AssignNode& node)
{
    SymbolID symbolId = resolveSymbol(node.identifier());
    const VariableInfo* targetVar = nullptr;
    const StructFieldInfo* memberField = nullptr;

    if (symbolId != InvalidSymbolID)
    {
        targetVar = &_symbols.at(symbolId);
        if (!targetVar->isMutable)
            addError("Variable '" + node.identifier() + "' is immutable", node);
        const_cast<AssignNode&>(node).setSymbolId(symbolId);
    }
    else if (_inFunction && !_currentMemberMaster.empty())
    {
        auto structIt = _structs.find(_currentMemberMaster);
        if (structIt != _structs.end())
        {
            for (const auto& field : structIt->second.fields)
            {
                if (field.name == node.identifier())
                {
                    memberField = &field;
                    break;
                }
            }
        }
    }

    if (!targetVar && !memberField)
        addError("Assignment to undeclared variable '" + node.identifier() + "'", node);

    if (const ExprNode* value = node.value())
    {
        value->accept(*this);
        if (targetVar)
        {
            if (targetVar->type.kind == TypeDesc::Kind::Builtin)
            {
                if (!isAssignable(targetVar->type.builtin, value->type()))
                    addError("Cannot assign incompatible type to variable '" + node.identifier() + "'", value);
            }
            else
            {
                std::string rhsStruct;
                if (!extractStructType(value, rhsStruct))
                    addError("Struct assignment requires a struct value", value);
                else if (rhsStruct != targetVar->type.structName)
                    addError("Struct assignment type mismatch", value);
            }
        }
        else if (memberField)
        {
            if (!memberField->isMutable)
                addError("Field '" + memberField->name + "' is immutable", node);
            if (memberField->type.kind == TypeDesc::Kind::Builtin)
            {
                if (!isAssignable(memberField->type.builtin, value->type()))
                    addError("Cannot assign incompatible type to field '" + memberField->name + "'", value);
            }
            else
            {
                std::string rhsStruct;
                if (!extractStructType(value, rhsStruct))
                    addError("Struct assignment requires a struct value", value);
                else if (rhsStruct != memberField->type.structName)
                    addError("Struct assignment type mismatch", value);
            }
        }
    }
}

void SemanticAnalyzer::visitAssignField(const AssignFieldNode& node)
{
    const FieldAccessNode* target = node.target();
    if (!target)
        return;

    SymbolID baseId = resolveSymbol(target->base());
    if (baseId == InvalidSymbolID)
    {
        addError("Assignment to undeclared variable '" + target->base() + "'", target);
        return;
    }

    const_cast<FieldAccessNode&>(*target).setBaseSymbolId(baseId);
    const VariableInfo& baseVar = _symbols.at(baseId);
    TypeDesc fieldType = resolveFieldType(baseId, target->fieldChain(), target);
    ensureFieldChainMutable(baseVar, target->fieldChain(), &node);

    if (const ExprNode* value = node.value())
    {
        value->accept(*this);
        if (fieldType.kind == TypeDesc::Kind::Builtin)
        {
            if (!isAssignable(fieldType.builtin, value->type()))
                addError("Cannot assign incompatible type to field", value);
        }
        else
        {
            std::string rhsStruct;
            if (!extractStructType(value, rhsStruct))
                addError("Struct field assignment requires a struct value", value);
            else if (rhsStruct != fieldType.structName)
                addError("Struct field assignment type mismatch", value);
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
            addError("Condition of if statement must be bool or integer", node);
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
    {
        if (_inFunction)
            addError("Return statement requires an expression", node);
        return;
    }

    node.expr()->accept(*this);
    if (_inFunction)
    {
        if (_currentFunctionReturn.kind == TypeDesc::Kind::Builtin)
        {
            if (!isAssignable(_currentFunctionReturn.builtin, node.expr()->type()))
                addError("Return type mismatch", node);
        }
        else
        {
            std::string retStruct;
            if (!extractStructType(node.expr(), retStruct))
                addError("Struct return value must be a struct", node);
            else if (retStruct != _currentFunctionReturn.structName)
                addError("Return struct type mismatch", node);
        }
    }
    else if (!canConvertToI32(node.expr()->type()))
    {
        addError("Return type must be convertible to i32", node);
    }
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
        if (!isNumeric(leftType) || !isNumeric(rightType))
        {
            addError("Arithmetic operators require numeric operands", node);
            node.setType(ValueType::Invalid);
            return;
        }
        node.setType(widerType(leftType, rightType));
        return;
    case BinaryOpNode::Operator::Equal:
    case BinaryOpNode::Operator::NotEqual:
    {
        ValueType operandType = comparisonOperandType(leftType, rightType);
        if (operandType == ValueType::Invalid)
        {
            addError("Comparison requires compatible operand types", node);
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
        addError("Logical not operator requires bool or integer operand", node);
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
        if (_inFunction && !_currentMemberMaster.empty())
        {
            auto structIt = _structs.find(_currentMemberMaster);
            if (structIt != _structs.end())
            {
                for (const auto& field : structIt->second.fields)
                {
                    if (field.name == node.name())
                    {
                        if (field.type.kind == TypeDesc::Kind::Builtin)
                            const_cast<IDNode&>(node).setType(field.type.builtin);
                        else
                            const_cast<IDNode&>(node).setType(ValueType::Invalid);
                        return;
                    }
                }
            }
        }

        addError("Use of undeclared variable '" + node.name() + "'", node);
        const_cast<IDNode&>(node).setType(ValueType::Invalid);
        return;
    }

    const VariableInfo& info = _symbols.at(symbolId);
    const_cast<IDNode&>(node).setSymbolId(symbolId);
    if (info.type.kind == TypeDesc::Kind::Builtin)
        const_cast<IDNode&>(node).setType(info.type.builtin);
    else
        const_cast<IDNode&>(node).setType(ValueType::Invalid);
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

void SemanticAnalyzer::visitStructLiteral(const StructLiteralNode& node)
{
    if (node.structType().kind != TypeDesc::Kind::Struct)
    {
        addError("Struct literal must reference a struct type", node);
        return;
    }

    auto structIt = _structs.find(node.structType().structName);
    if (structIt == _structs.end())
    {
        addError("Unknown struct type '" + node.structType().structName + "'", node);
        return;
    }

    const auto& fields = structIt->second.fields;
    if (node.args().size() > fields.size())
        addError("Struct literal provides too many arguments", node);

    size_t limit = std::min(node.args().size(), fields.size());
    for (size_t i = 0; i < limit; ++i)
    {
        const ExprNode* arg = node.args()[i].get();
        if (!arg)
            continue;
        arg->accept(*this);
        const auto& field = fields[i];
        if (field.type.kind == TypeDesc::Kind::Builtin)
        {
            if (!isAssignable(field.type.builtin, arg->type()))
                addError("Struct literal argument type mismatch for field '" + field.name + "'", arg);
        }
        else
        {
            std::string argStruct;
            if (!extractStructType(arg, argStruct))
                addError("Struct literal argument for field '" + field.name + "' must be a struct", arg);
            else if (argStruct != field.type.structName)
                addError("Struct literal argument type mismatch for field '" + field.name + "'", arg);
        }
    }
}

void SemanticAnalyzer::visitFieldAccess(const FieldAccessNode& node)
{
    SymbolID baseId = resolveSymbol(node.base());
    if (baseId == InvalidSymbolID)
    {
        addError("Use of undeclared variable '" + node.base() + "'", node);
        return;
    }

    const_cast<FieldAccessNode&>(node).setBaseSymbolId(baseId);
    TypeDesc fieldType = resolveFieldType(baseId, node.fieldChain(), &node);
    if (fieldType.kind == TypeDesc::Kind::Builtin)
        const_cast<FieldAccessNode&>(node).setType(fieldType.builtin);
    else
        const_cast<FieldAccessNode&>(node).setType(ValueType::Invalid);
}

void SemanticAnalyzer::visitFunctionCall(const FunctionCallNode& node)
{
    auto funcIt = _functions.find(node.name());
    if (funcIt == _functions.end())
    {
        SymbolID sid = resolveSymbol(node.name());
        if (sid != InvalidSymbolID)
        {
            const auto& var = _symbols.at(sid);
            if (var.type.kind == TypeDesc::Kind::Struct)
            {
                const FunctionInfo* callInfo = findMemberFunction("call", var.type.structName);
                if (callInfo)
                {
                    const_cast<FunctionCallNode&>(node).setSymbolId(sid);
                    validateCallArguments(node.args(), *callInfo, 1, "Use of undeclared variable in callable object");
                    if (callInfo->returnType.kind == TypeDesc::Kind::Builtin)
                        const_cast<FunctionCallNode&>(node).setType(callInfo->returnType.builtin);
                    else
                        const_cast<FunctionCallNode&>(node).setType(ValueType::Invalid);
                    return;
                }
            }
        }

        addError("Call to undeclared function '" + node.name() + "'", node);
        return;
    }

    const FunctionInfo& func = funcIt->second;
    if (node.args().size() != func.params.size())
    {
        addError("Function '" + node.name() + "' called with wrong number of arguments", node);
        return;
    }

    validateCallArguments(node.args(), func, 0, "Use of undeclared variable in function call");
    if (func.returnType.kind == TypeDesc::Kind::Builtin)
        const_cast<FunctionCallNode&>(node).setType(func.returnType.builtin);
    else
        const_cast<FunctionCallNode&>(node).setType(ValueType::Invalid);
}

void SemanticAnalyzer::visitMemberFunctionCall(const MemberFunctionCallNode& node)
{
    SymbolID baseId = resolveSymbol(node.base());
    if (baseId == InvalidSymbolID)
    {
        addError("Use of undeclared variable '" + node.base() + "'", node);
        return;
    }

    const_cast<MemberFunctionCallNode&>(node).setBaseSymbolId(baseId);
    TypeDesc masterType = resolveFieldType(baseId, node.fieldChain(), &node);
    if (masterType.kind != TypeDesc::Kind::Struct)
    {
        addError("Member function base must be a struct", node);
        return;
    }

    const FunctionInfo* funcInfo = findMemberFunction(node.funcName(), masterType.structName);
    if (!funcInfo)
    {
        addError("Call to undeclared member function '" + node.funcName() + "'", node);
        return;
    }

    if (funcInfo->params.empty())
    {
        addError("Corrupt member function metadata", node);
        return;
    }

    size_t expectedArgs = funcInfo->params.size() - 1;
    if (node.args().size() != expectedArgs)
    {
        addError("Member function '" + node.funcName() + "' called with wrong number of arguments", node);
        return;
    }

    validateCallArguments(node.args(), *funcInfo, 1, "Use of undeclared variable in member function call");
    if (funcInfo->returnType.kind == TypeDesc::Kind::Builtin)
        const_cast<MemberFunctionCallNode&>(node).setType(funcInfo->returnType.builtin);
    else
        const_cast<MemberFunctionCallNode&>(node).setType(ValueType::Invalid);
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
    return _scopeStack.empty() ? 0u : _scopeStack.back();
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

void SemanticAnalyzer::addError(const std::string& message, const ASTNode& node)
{
    _errors.push_back(formatMessage(message, node.line()));
}

void SemanticAnalyzer::addError(const std::string& message, const ASTNode* node)
{
    _errors.push_back(formatMessage(message, node ? node->line() : 0));
}

void SemanticAnalyzer::addWarning(const std::string& message)
{
    _warnings.push_back(message);
}

void SemanticAnalyzer::addWarning(const std::string& message, const ASTNode& node)
{
    _warnings.push_back(formatMessage(message, node.line()));
}

void SemanticAnalyzer::addWarning(const std::string& message, const ASTNode* node)
{
    _warnings.push_back(formatMessage(message, node ? node->line() : 0));
}

void SemanticAnalyzer::validateCallArguments(const std::vector<std::unique_ptr<ExprNode>>& args,
                                             const FunctionInfo& funcInfo,
                                             size_t paramStartIndex,
                                             const std::string& undeclaredVarMessage)
{
    for (size_t i = 0; i < args.size(); ++i)
    {
        const ExprNode* arg = args[i] ? args[i].get() : nullptr;
        if (!arg)
            continue;

        arg->accept(*this);
        if (paramStartIndex + i >= funcInfo.params.size())
        {
            addError("Argument type mismatch at position " + std::to_string(i), arg);
            continue;
        }

        const auto& param = funcInfo.params[paramStartIndex + i];
        if (param.type.kind == TypeDesc::Kind::Builtin)
        {
            if (!isAssignable(param.type.builtin, arg->type()))
                addError("Argument type mismatch at position " + std::to_string(i), arg);
        }
        else
        {
            std::string argStruct;
            if (!extractStructType(arg, argStruct))
            {
                addError("Struct argument must evaluate to type '" + param.type.structName + "'", arg);
            }
            else if (argStruct != param.type.structName)
            {
                addError("Struct argument type mismatch", arg);
            }
        }
    }
}

bool SemanticAnalyzer::extractStructType(const ExprNode* expr, std::string& outStructName) const
{
    if (!expr)
        return false;

    if (const auto* id = dynamic_cast<const IDNode*>(expr))
    {
        if (id->symbolId() == InvalidSymbolID)
            return false;
        auto symIt = _symbols.find(id->symbolId());
        if (symIt == _symbols.end())
            return false;
        if (symIt->second.type.kind == TypeDesc::Kind::Struct)
        {
            outStructName = symIt->second.type.structName;
            return true;
        }
        return false;
    }

    if (const auto* literal = dynamic_cast<const StructLiteralNode*>(expr))
    {
        if (literal->structType().kind == TypeDesc::Kind::Struct)
        {
            outStructName = literal->structType().structName;
            return true;
        }
        return false;
    }

    return false;
}

const FunctionInfo* SemanticAnalyzer::findMemberFunction(const std::string& funcName, const std::string& structName) const
{
    auto it = _functions.find(funcName);
    if (it != _functions.end() && it->second.isMember && it->second.masterStruct == structName)
        return &it->second;

    for (const auto& entry : _functions)
    {
        if (entry.second.name == funcName && entry.second.isMember && entry.second.masterStruct == structName)
            return &entry.second;
    }
    return nullptr;
}

TypeDesc SemanticAnalyzer::resolveFieldType(SymbolID baseId,
                                            const std::vector<std::string>& fieldChain,
                                            const ASTNode* reporter)
{
    auto baseIt = _symbols.find(baseId);
    if (baseIt == _symbols.end())
    {
        addError("Unknown symbol reference", reporter);
        return TypeDesc::Builtin(ValueType::Invalid);
    }

    if (baseIt->second.type.kind != TypeDesc::Kind::Struct)
    {
        addError("Variable '" + baseIt->second.name + "' is not a struct", reporter);
        return TypeDesc::Builtin(ValueType::Invalid);
    }

    TypeDesc current = baseIt->second.type;
    if (fieldChain.empty())
        return current;

    for (const std::string& fieldName : fieldChain)
    {
        auto structIt = _structs.find(current.structName);
        if (structIt == _structs.end())
        {
            addError("Unknown struct type '" + current.structName + "'", reporter);
            return TypeDesc::Builtin(ValueType::Invalid);
        }

        const StructFieldInfo* match = nullptr;
        for (const auto& field : structIt->second.fields)
        {
            if (field.name == fieldName)
            {
                match = &field;
                break;
            }
        }

        if (!match)
        {
            addError("Struct '" + structIt->second.name + "' has no field '" + fieldName + "'", reporter);
            return TypeDesc::Builtin(ValueType::Invalid);
        }

        current = match->type;
    }

    return current;
}

bool SemanticAnalyzer::ensureFieldChainMutable(const VariableInfo& baseVar,
                                               const std::vector<std::string>& fieldChain,
                                               const ASTNode* reporter)
{
    if (!baseVar.isMutable)
    {
        addError("Variable '" + baseVar.name + "' is immutable", reporter);
        return false;
    }

    TypeDesc current = baseVar.type;
    const StructFieldInfo* match = nullptr;
    for (const std::string& fieldName : fieldChain)
    {
        if (current.kind != TypeDesc::Kind::Struct)
        {
            addError("Cannot access field '" + fieldName + "' on non-struct value", reporter);
            return false;
        }

        auto structIt = _structs.find(current.structName);
        if (structIt == _structs.end())
        {
            addError("Unknown struct type '" + current.structName + "'", reporter);
            return false;
        }

        match = nullptr;
        for (const auto& field : structIt->second.fields)
        {
            if (field.name == fieldName)
            {
                match = &field;
                break;
            }
        }

        if (!match)
        {
            addError("Struct '" + structIt->second.name + "' has no field '" + fieldName + "'", reporter);
            return false;
        }

        if (!match->isMutable)
        {
            addError("Field '" + fieldName + "' is immutable", reporter);
            return false;
        }

        current = match->type;
    }

    return true;
}

void SemanticAnalyzer::registerBuiltins()
{
    FunctionInfo read;
    read.name = "__builtin_read";
    read.returnType = TypeDesc::Builtin(ValueType::I32);
    read.isBuiltin = true;
    _functions.emplace(read.name, std::move(read));

    FunctionInfo write;
    write.name = "__builtin_write";
    write.returnType = TypeDesc::Builtin(ValueType::I32);
    write.isBuiltin = true;
    write.params.push_back(FunctionParamInfo{ TypeDesc::Builtin(ValueType::I32), "value", InvalidSymbolID });
    _functions.emplace(write.name, std::move(write));
}
