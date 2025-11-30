#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "AST.hpp"

struct TemplateSubstitution
{
    std::string placeholder;
    TypeDesc replacement;
};

class ASTCloner
{
public:
    using ScopeAllocator = std::function<size_t()>;

    ASTCloner(TemplateSubstitution substitution, ScopeAllocator allocator);

    std::unique_ptr<FunctionNode> cloneFunction(const FunctionNode& original, std::string newName);

private:
    TypeDesc substitute(const TypeDesc& type) const;
    std::unique_ptr<BlockNode> cloneBlock(const BlockNode& block);
    std::unique_ptr<StmtNode> cloneStmt(const StmtNode& stmt);
    std::unique_ptr<ExprNode> cloneExpr(const ExprNode& expr);
    std::vector<std::unique_ptr<ExprNode>> cloneExprList(const std::vector<std::unique_ptr<ExprNode>>& list);

    size_t remapScope(size_t originalId);

    TemplateSubstitution _subst;
    ScopeAllocator _scopeAllocator;
    std::unordered_map<size_t, size_t> _scopeMap;
};