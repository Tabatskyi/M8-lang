#pragma once

#include <functional>
#include <unordered_map>

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

    template<typename T>
    std::unique_ptr<T> clone(const T& node);

    template<typename T>
    std::vector<std::unique_ptr<T>> cloneList(const std::vector<std::unique_ptr<T>>& list);

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

template<typename T>
std::unique_ptr<T> ASTCloner::clone(const T& node)
{
    if constexpr (std::is_base_of_v<ExprNode, T>)
    {
        return std::unique_ptr<T>(dynamic_cast<T*>(cloneExpr(node).release()));
    }
    else if constexpr (std::is_base_of_v<StmtNode, T>)
    {
        return std::unique_ptr<T>(dynamic_cast<T*>(cloneStmt(node).release()));
    }
    else if constexpr (std::is_same_v<T, BlockNode>)
    {
        return cloneBlock(node);
    }
    return nullptr;
}

template<typename T>
std::vector<std::unique_ptr<T>> ASTCloner::cloneList(const std::vector<std::unique_ptr<T>>& list)
{
    std::vector<std::unique_ptr<T>> result;
    result.reserve(list.size());
    for (const auto& item : list)
    {
        if (!item)
            continue;
        auto cloned = clone(*item);
        if (cloned)
            result.push_back(std::move(cloned));
    }
    return result;
}