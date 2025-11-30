#include "ASTCloner.hpp"

#include <utility>
#include <vector>

ASTCloner::ASTCloner(TemplateSubstitution substitution, ScopeAllocator allocator)
    : _subst(std::move(substitution)), _scopeAllocator(std::move(allocator))
{
}

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
        body = cloneBlock(*originalBody);

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
    BlockNode::StmtList statements;
    statements.reserve(block.statements().size());
    for (const auto& stmt : block.statements())
    {
        if (!stmt)
            continue;
        auto clone = cloneStmt(*stmt);
        if (clone)
            statements.push_back(std::move(clone));
    }
    size_t newScope = remapScope(block.scopeId());
    auto clone = std::make_unique<BlockNode>(std::move(statements), newScope);
    clone->setLine(block.line());
    return clone;
}

std::unique_ptr<StmtNode> ASTCloner::cloneStmt(const StmtNode& stmt)
{
    if (auto* decl = dynamic_cast<const DeclNode*>(&stmt))
    {
        auto inits = cloneExprList(decl->initializers());
        auto clone = std::make_unique<DeclNode>(substitute(decl->declaredType()), decl->identifier(), decl->isMutable(), std::move(inits));
        clone->setLine(stmt.line());
        clone->setSymbolId(InvalidSymbolID);
        return clone;
    }
    if (auto* assign = dynamic_cast<const AssignNode*>(&stmt))
    {
        std::unique_ptr<ExprNode> value;
        if (const ExprNode* expr = assign->value())
            value = cloneExpr(*expr);
        auto clone = std::make_unique<AssignNode>(assign->identifier(), std::move(value));
        clone->setLine(stmt.line());
        clone->setSymbolId(InvalidSymbolID);
        return clone;
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
            value = cloneExpr(*expr);
        auto clone = std::make_unique<AssignFieldNode>(std::move(target), std::move(value));
        clone->setLine(stmt.line());
        return clone;
    }
    if (auto* ifNode = dynamic_cast<const IfNode*>(&stmt))
    {
        std::unique_ptr<ExprNode> condition;
        if (const ExprNode* cond = ifNode->condition())
            condition = cloneExpr(*cond);
        std::unique_ptr<BlockNode> thenBlock;
        if (const BlockNode* tb = ifNode->thenBlock())
            thenBlock = cloneBlock(*tb);
        std::unique_ptr<BlockNode> elseBlock;
        if (const BlockNode* eb = ifNode->elseBlock())
            elseBlock = cloneBlock(*eb);
        auto clone = std::make_unique<IfNode>(std::move(condition), std::move(thenBlock), std::move(elseBlock));
        clone->setLine(stmt.line());
        return clone;
    }
    if (auto* ret = dynamic_cast<const ReturnNode*>(&stmt))
    {
        std::unique_ptr<ExprNode> value;
        if (const ExprNode* expr = ret->expr())
            value = cloneExpr(*expr);
        auto clone = std::make_unique<ReturnNode>(std::move(value));
        clone->setLine(stmt.line());
        return clone;
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

        auto clone = std::make_unique<StructDeclNode>(structDecl->name(), std::move(fields), std::move(methods));
        clone->setLine(stmt.line());
        return clone;
    }
    if (auto* function = dynamic_cast<const FunctionNode*>(&stmt))
    {
        auto clone = cloneFunction(*function, function->name());
        return clone;
    }
    return nullptr;
}

std::unique_ptr<ExprNode> ASTCloner::cloneExpr(const ExprNode& expr)
{
    if (auto* bin = dynamic_cast<const BinaryOpNode*>(&expr))
    {
        std::unique_ptr<ExprNode> left;
        if (const ExprNode* l = bin->left())
            left = cloneExpr(*l);
        std::unique_ptr<ExprNode> right;
        if (const ExprNode* r = bin->right())
            right = cloneExpr(*r);
        auto clone = std::make_unique<BinaryOpNode>(bin->op(), std::move(left), std::move(right));
        clone->setLine(expr.line());
        return clone;
    }
    if (auto* un = dynamic_cast<const UnaryOpNode*>(&expr))
    {
        std::unique_ptr<ExprNode> operand;
        if (const ExprNode* op = un->operand())
            operand = cloneExpr(*op);
        auto clone = std::make_unique<UnaryOpNode>(un->op(), std::move(operand));
        clone->setLine(expr.line());
        return clone;
    }
    if (auto* call = dynamic_cast<const FunctionCallNode*>(&expr))
    {
        std::vector<std::unique_ptr<ExprNode>> args = cloneExprList(call->args());
        auto clone = std::make_unique<FunctionCallNode>(call->name(), std::move(args));
        clone->setSymbolId(InvalidSymbolID);
        clone->setLine(expr.line());
        return clone;
    }
    if (auto* memberCall = dynamic_cast<const MemberFunctionCallNode*>(&expr))
    {
        std::vector<std::unique_ptr<ExprNode>> args = cloneExprList(memberCall->args());
        auto clone = std::make_unique<MemberFunctionCallNode>(memberCall->base(), memberCall->fieldChain(), memberCall->funcName(), std::move(args));
        clone->setBaseSymbolId(InvalidSymbolID);
        clone->setLine(expr.line());
        return clone;
    }
    if (auto* id = dynamic_cast<const IDNode*>(&expr))
    {
        auto clone = std::make_unique<IDNode>(id->name());
        clone->setSymbolId(InvalidSymbolID);
        clone->setLine(expr.line());
        return clone;
    }
    if (auto* num = dynamic_cast<const NumberNode*>(&expr))
    {
        auto clone = std::make_unique<NumberNode>(num->value());
        clone->setLine(expr.line());
        return clone;
    }
    if (auto* boolean = dynamic_cast<const BoolLiteralNode*>(&expr))
    {
        auto clone = std::make_unique<BoolLiteralNode>(boolean->value());
        clone->setLine(expr.line());
        return clone;
    }
    if (auto* stringLit = dynamic_cast<const StringLiteralNode*>(&expr))
    {
        auto clone = std::make_unique<StringLiteralNode>(stringLit->value());
        clone->setLine(expr.line());
        return clone;
    }
    if (auto* structLiteral = dynamic_cast<const StructLiteralNode*>(&expr))
    {
        auto args = cloneExprList(structLiteral->args());
        auto clone = std::make_unique<StructLiteralNode>(substitute(structLiteral->structType()), std::move(args));
        clone->setLine(expr.line());
        return clone;
    }
    if (auto* field = dynamic_cast<const FieldAccessNode*>(&expr))
    {
        auto clone = std::make_unique<FieldAccessNode>(field->base(), field->fieldChain());
        clone->setBaseSymbolId(InvalidSymbolID);
        clone->setLine(expr.line());
        return clone;
    }
    return nullptr;
}

std::vector<std::unique_ptr<ExprNode>> ASTCloner::cloneExprList(const std::vector<std::unique_ptr<ExprNode>>& list)
{
    std::vector<std::unique_ptr<ExprNode>> result;
    result.reserve(list.size());
    for (const auto& expr : list)
    {
        if (!expr)
            continue;
        auto clone = cloneExpr(*expr);
        if (clone)
            result.push_back(std::move(clone));
    }
    return result;
}