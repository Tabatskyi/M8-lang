#include "ASTCloner.hpp"

ASTCloner::ASTCloner(TemplateSubstitution substitution, ScopeAllocator allocator)
    : _subst(std::move(substitution)), _scopeAllocator(std::move(allocator)) {}

std::unique_ptr<FunctionNode> ASTCloner::cloneFunction(const FunctionNode& original, std::string newName)
{
    std::vector<FunctionNode::Param> params;
    params.reserve(original.params().size());
    for (const auto& param : original.params())
    {
        FunctionNode::Param copy{ substitute(param.type), param.name, InvalidSymbolID };
        params.push_back(std::move(copy));
    }

    std::unique_ptr<BlockNode> body;
    if (const BlockNode* originalBody = original.body())
        body = clone<BlockNode>(*originalBody);

    size_t newScope = remapScope(original.scopeId());
    TypeDesc returnType = substitute(original.returnType());

    auto fn = std::make_unique<FunctionNode>(std::move(newName), std::move(params), std::move(returnType), std::move(body), newScope, original.masterStruct(), false);
    fn->setLine(original.line());
    return fn;
}

TypeDesc ASTCloner::substitute(const TypeDesc& type) const
{
    if (type.kind == TypeDesc::Kind::TemplateParam && type.templateName == _subst.placeholder)
        return _subst.replacement;
    return type;
}

size_t ASTCloner::remapScope(size_t originalId)
{
    auto it = _scopeMap.find(originalId);
    if (it != _scopeMap.end())
        return it->second;

    size_t newId = _scopeAllocator ? _scopeAllocator() : originalId;
    _scopeMap.emplace(originalId, newId);
    return newId;
}

std::unique_ptr<BlockNode> ASTCloner::cloneBlock(const BlockNode& block)
{
    auto statements = cloneList<StmtNode>(block.statements());
    size_t newScope = remapScope(block.scopeId());
    auto cloned = std::make_unique<BlockNode>(std::move(statements), newScope);
    cloned->setLine(block.line());
    return cloned;
}

std::unique_ptr<StmtNode> ASTCloner::cloneStmt(const StmtNode& stmt)
{
    if (auto* decl = dynamic_cast<const DeclNode*>(&stmt))
    {
        auto inits = cloneList<ExprNode>(decl->initializers());
        auto cloned = std::make_unique<DeclNode>(substitute(decl->declaredType()), decl->identifier(), decl->isMutable(), std::move(inits));
        cloned->setLine(stmt.line());
        cloned->setSymbolId(InvalidSymbolID);
        return cloned;
    }
    if (auto* assign = dynamic_cast<const AssignNode*>(&stmt))
    {
        std::unique_ptr<ExprNode> value;
        if (const ExprNode* expr = assign->value())
            value = clone<ExprNode>(*expr);
        auto cloned = std::make_unique<AssignNode>(assign->identifier(), std::move(value));
        cloned->setLine(stmt.line());
        cloned->setSymbolId(InvalidSymbolID);
        return cloned;
    }
    if (auto* assignField = dynamic_cast<const AssignFieldNode*>(&stmt))
    {
        std::unique_ptr<FieldAccessNode> target;
        if (const FieldAccessNode* field = assignField->target())
        {
            target = std::make_unique<FieldAccessNode>(field->base(), field->fieldChain());
            target->setLine(field->line());
            target->setBaseSymbolId(InvalidSymbolID);
        }
        std::unique_ptr<ExprNode> value;
        if (const ExprNode* expr = assignField->value())
            value = clone<ExprNode>(*expr);
        auto cloned = std::make_unique<AssignFieldNode>(std::move(target), std::move(value));
        cloned->setLine(stmt.line());
        return cloned;
    }
    if (auto* ifNode = dynamic_cast<const IfNode*>(&stmt))
    {
        std::unique_ptr<ExprNode> condition;
        if (const ExprNode* cond = ifNode->condition())
            condition = clone<ExprNode>(*cond);
        std::unique_ptr<BlockNode> thenBlock;
        if (const BlockNode* tb = ifNode->thenBlock())
            thenBlock = clone<BlockNode>(*tb);
        std::unique_ptr<BlockNode> elseBlock;
        if (const BlockNode* eb = ifNode->elseBlock())
            elseBlock = clone<BlockNode>(*eb);
        auto cloned = std::make_unique<IfNode>(std::move(condition), std::move(thenBlock), std::move(elseBlock));
        cloned->setLine(stmt.line());
        return cloned;
    }
    if (auto* exprStmt = dynamic_cast<const ExprStmtNode*>(&stmt))
    {
        std::unique_ptr<ExprNode> expr;
        if (const ExprNode* inner = exprStmt->expr())
            expr = clone<ExprNode>(*inner);
        auto cloned = std::make_unique<ExprStmtNode>(std::move(expr));
        cloned->setLine(stmt.line());
        return cloned;
    }
    if (auto* ret = dynamic_cast<const ReturnNode*>(&stmt))
    {
        std::unique_ptr<ExprNode> value;
        if (const ExprNode* expr = ret->expr())
            value = clone<ExprNode>(*expr);
        auto cloned = std::make_unique<ReturnNode>(std::move(value));
        cloned->setLine(stmt.line());
        return cloned;
    }
    if (auto* structDecl = dynamic_cast<const StructDeclNode*>(&stmt))
    {
        std::vector<StructDeclNode::Field> fields = structDecl->fields();
        for (auto& field : fields)
            field.type = substitute(field.type);

        std::vector<std::unique_ptr<FunctionNode>> methods;
        methods.reserve(structDecl->functions().size());
        for (const auto& method : structDecl->functions())
        {
            if (!method)
                continue;
            auto methodClone = cloneFunction(*method, method->name());
            if (methodClone)
                methods.push_back(std::move(methodClone));
        }

        auto cloned = std::make_unique<StructDeclNode>(structDecl->name(), std::move(fields), std::move(methods));
        cloned->setLine(stmt.line());
        return cloned;
    }
    if (auto* function = dynamic_cast<const FunctionNode*>(&stmt))
    {
        auto cloned = cloneFunction(*function, function->name());
        return cloned;
    }
    return nullptr;
}

std::unique_ptr<ExprNode> ASTCloner::cloneExpr(const ExprNode& expr)
{
    if (auto* bin = dynamic_cast<const BinaryOpNode*>(&expr))
    {
        std::unique_ptr<ExprNode> left;
        if (const ExprNode* l = bin->left())
            left = clone<ExprNode>(*l);
        std::unique_ptr<ExprNode> right;
        if (const ExprNode* r = bin->right())
            right = clone<ExprNode>(*r);
        auto cloned = std::make_unique<BinaryOpNode>(bin->op(), std::move(left), std::move(right));
        cloned->setLine(expr.line());
        return cloned;
    }
    if (auto* un = dynamic_cast<const UnaryOpNode*>(&expr))
    {
        std::unique_ptr<ExprNode> operand;
        if (const ExprNode* op = un->operand())
            operand = clone<ExprNode>(*op);
        auto cloned = std::make_unique<UnaryOpNode>(un->op(), std::move(operand));
        cloned->setLine(expr.line());
        return cloned;
    }
    if (auto* call = dynamic_cast<const FunctionCallNode*>(&expr))
    {
        auto args = cloneList<ExprNode>(call->args());
        auto cloned = std::make_unique<FunctionCallNode>(call->name(), std::move(args));
        cloned->setSymbolId(InvalidSymbolID);
        cloned->setLine(expr.line());
        return cloned;
    }
    if (auto* memberCall = dynamic_cast<const MemberFunctionCallNode*>(&expr))
    {
        auto args = cloneList<ExprNode>(memberCall->args());
        auto cloned = std::make_unique<MemberFunctionCallNode>(memberCall->base(), memberCall->fieldChain(), memberCall->funcName(), std::move(args));
        cloned->setBaseSymbolId(InvalidSymbolID);
        cloned->setLine(expr.line());
        return cloned;
    }
    if (auto* id = dynamic_cast<const IDNode*>(&expr))
    {
        auto cloned = std::make_unique<IDNode>(id->name());
        cloned->setSymbolId(InvalidSymbolID);
        cloned->setLine(expr.line());
        return cloned;
    }
    if (auto* num = dynamic_cast<const NumberNode*>(&expr))
    {
        auto cloned = std::make_unique<NumberNode>(num->value());
        cloned->setLine(expr.line());
        return cloned;
    }
    if (auto* boolean = dynamic_cast<const BoolLiteralNode*>(&expr))
    {
        auto cloned = std::make_unique<BoolLiteralNode>(boolean->value());
        cloned->setLine(expr.line());
        return cloned;
    }
    if (auto* stringLit = dynamic_cast<const StringLiteralNode*>(&expr))
    {
        auto cloned = std::make_unique<StringLiteralNode>(stringLit->value());
        cloned->setLine(expr.line());
        return cloned;
    }
    if (auto* structLiteral = dynamic_cast<const StructLiteralNode*>(&expr))
    {
        auto args = cloneList<ExprNode>(structLiteral->args());
        auto cloned = std::make_unique<StructLiteralNode>(substitute(structLiteral->structType()), std::move(args));
        cloned->setLine(expr.line());
        return cloned;
    }
    if (auto* field = dynamic_cast<const FieldAccessNode*>(&expr))
    {
        auto cloned = std::make_unique<FieldAccessNode>(field->base(), field->fieldChain());
        cloned->setBaseSymbolId(InvalidSymbolID);
        cloned->setLine(expr.line());
        return cloned;
    }
    return nullptr;
}

std::vector<std::unique_ptr<ExprNode>> ASTCloner::cloneExprList(const std::vector<std::unique_ptr<ExprNode>>& list)
{
    return cloneList<ExprNode>(list);
}