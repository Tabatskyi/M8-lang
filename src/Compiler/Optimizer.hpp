#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../AST/AST.hpp"
#include "../General/Utility.hpp"

class SemanticAnalyzer;

class Optimizer
{
public:
    void run(ProgramNode& program, const SemanticAnalyzer& semantic);

private:
    struct InlinePlan
    {
        const FunctionNode* function = nullptr;
        const ReturnNode* returnStmt = nullptr;
        const ExprNode* returnExpr = nullptr;
        std::vector<SymbolID> paramOrder;
    };

    using SymbolUsageMap = std::unordered_map<SymbolID, std::size_t>;
    using InlinePlanMap = std::unordered_map<std::string, InlinePlan>;
    using FunctionCallCountMap = std::unordered_map<std::string, std::size_t>;
    using SubstitutionMap = std::unordered_map<SymbolID, const ExprNode*>;

    void eliminateUnusedVariables(ProgramNode& program) const;
    bool eliminateUnusedVariablesPass(ProgramNode& program) const;
    SymbolUsageMap collectVariableUsage(const ProgramNode& program) const;
    void collectVariableUsage(const StmtNode& stmt, SymbolUsageMap& usage) const;
    void collectVariableUsage(const ExprNode* expr, SymbolUsageMap& usage) const;
    void collectVariableUsage(const BlockNode* block, SymbolUsageMap& usage) const;
    void collectUnusedDecls(const std::vector<std::unique_ptr<StmtNode>>& statements,
                            const SymbolUsageMap& usage,
                            std::unordered_set<SymbolID>& unused) const;
    void collectUnusedDecls(const BlockNode* block,
                            const SymbolUsageMap& usage,
                            std::unordered_set<SymbolID>& unused) const;
    bool pruneStatements(std::vector<std::unique_ptr<StmtNode>>& statements,
                         const std::unordered_set<SymbolID>& unused) const;
    bool pruneStatement(StmtNode& stmt, const std::unordered_set<SymbolID>& unused) const;

    InlinePlanMap planInlines(const ProgramNode& program, const SemanticAnalyzer& semantic) const;
    bool isInlineable(const FunctionNode& node, InlinePlan& plan) const;
    bool validateParameterUsage(const ExprNode& expr,
                                const std::unordered_set<SymbolID>& params,
                                std::unordered_map<SymbolID, std::size_t>& usage) const;
    void applyInlines(ProgramNode& program, const InlinePlanMap& plans) const;
    void applyInlinesToStatements(std::vector<std::unique_ptr<StmtNode>>& statements,
                                  const InlinePlanMap& plans) const;
    std::unique_ptr<ExprNode> transformExpr(const ExprNode* expr,
                                            const InlinePlanMap& plans,
                                            const SubstitutionMap* substitution) const;
    std::unique_ptr<ExprNode> inlineCall(const FunctionCallNode& call,
                                         const InlinePlan& plan,
                                         const InlinePlanMap& plans) const;

    void removeUnusedFunctions(ProgramNode& program) const;
    FunctionCallCountMap countFunctionCalls(const ProgramNode& program) const;
    void countFunctionCalls(const StmtNode& stmt, FunctionCallCountMap& counts) const;
    void countFunctionCalls(const ExprNode* expr, FunctionCallCountMap& counts) const;
    void countFunctionCalls(const BlockNode* block, FunctionCallCountMap& counts) const;
};
