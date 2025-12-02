#include "Optimizer.hpp"

void Optimizer::run(ProgramNode& program, const SemanticAnalyzer& semantic)
{
	(void)semantic;
	eliminateUnusedVariables(program);
}

void Optimizer::eliminateUnusedVariables(ProgramNode& program) const
{
	while (eliminateUnusedVariablesPass(program))
		;
}

bool Optimizer::eliminateUnusedVariablesPass(ProgramNode& program) const
{
	SymbolUsageMap usage = collectVariableUsage(program);

	std::unordered_set<SymbolID> unused;
	collectUnusedDecls(program.statements(), usage, unused);

	if (unused.empty())
		return false;

	return pruneStatements(program.statements(), unused);
}

Optimizer::SymbolUsageMap Optimizer::collectVariableUsage(const ProgramNode& program) const
{
	SymbolUsageMap usage;
	for (const auto& stmt : program.statements())
	{
		if (stmt)
			collectVariableUsage(*stmt, usage);
	}
	return usage;
}

void Optimizer::collectVariableUsage(const StmtNode& stmt, SymbolUsageMap& usage) const
{
	if (const auto* decl = dynamic_cast<const DeclNode*>(&stmt))
	{
		for (const auto& init : decl->initializers())
			collectVariableUsage(init.get(), usage);
		return;
	}

	if (const auto* assign = dynamic_cast<const AssignNode*>(&stmt))
	{
		collectVariableUsage(assign->value(), usage);
		return;
	}

	if (const auto* assignField = dynamic_cast<const AssignFieldNode*>(&stmt))
	{
		collectVariableUsage(assignField->target(), usage);
		collectVariableUsage(assignField->value(), usage);
		return;
	}

	if (const auto* exprStmt = dynamic_cast<const ExprStmtNode*>(&stmt))
	{
		collectVariableUsage(exprStmt->expr(), usage);
		return;
	}

	if (const auto* ifNode = dynamic_cast<const IfNode*>(&stmt))
	{
		collectVariableUsage(ifNode->condition(), usage);
		collectVariableUsage(ifNode->thenBlock(), usage);
		collectVariableUsage(ifNode->elseBlock(), usage);
		return;
	}

	if (const auto* ret = dynamic_cast<const ReturnNode*>(&stmt))
	{
		collectVariableUsage(ret->expr(), usage);
		return;
	}

	if (const auto* fn = dynamic_cast<const FunctionNode*>(&stmt))
	{
		collectVariableUsage(fn->body(), usage);
		return;
	}

	if (const auto* structDecl = dynamic_cast<const StructDeclNode*>(&stmt))
	{
		for (const auto& method : structDecl->functions())
		{
			if (method)
				collectVariableUsage(method->body(), usage);
		}
		return;
	}
}

void Optimizer::collectVariableUsage(const BlockNode* block, SymbolUsageMap& usage) const
{
	if (!block)
		return;

	for (const auto& stmt : block->statements())
	{
		if (stmt)
			collectVariableUsage(*stmt, usage);
	}
}

void Optimizer::collectVariableUsage(const ExprNode* expr, SymbolUsageMap& usage) const
{
	if (!expr)
		return;

	if (const auto* id = dynamic_cast<const IDNode*>(expr))
	{
		const SymbolID symbol = id->symbolId();
		if (symbol != InvalidSymbolID)
			++usage[symbol];
		return;
	}

	if (const auto* field = dynamic_cast<const FieldAccessNode*>(expr))
	{
		const SymbolID baseSymbol = field->baseSymbolId();
		if (baseSymbol != InvalidSymbolID)
			++usage[baseSymbol];
		return;
	}

	if (const auto* call = dynamic_cast<const FunctionCallNode*>(expr))
	{
		for (const auto& arg : call->args())
			collectVariableUsage(arg.get(), usage);
		return;
	}

	if (const auto* memberCall = dynamic_cast<const MemberFunctionCallNode*>(expr))
	{
		const SymbolID baseSymbol = memberCall->baseSymbolId();
		if (baseSymbol != InvalidSymbolID)
			++usage[baseSymbol];

		for (const auto& arg : memberCall->args())
			collectVariableUsage(arg.get(), usage);
		return;
	}

	if (const auto* binary = dynamic_cast<const BinaryOpNode*>(expr))
	{
		collectVariableUsage(binary->left(), usage);
		collectVariableUsage(binary->right(), usage);
		return;
	}

	if (const auto* unary = dynamic_cast<const UnaryOpNode*>(expr))
	{
		collectVariableUsage(unary->operand(), usage);
		return;
	}

	if (const auto* structLiteral = dynamic_cast<const StructLiteralNode*>(expr))
	{
		for (const auto& arg : structLiteral->args())
			collectVariableUsage(arg.get(), usage);
		return;
	}

	if (const auto* boolLiteral = dynamic_cast<const BoolLiteralNode*>(expr))
	{
		(void)boolLiteral;
		return;
	}

	if (const auto* stringLiteral = dynamic_cast<const StringLiteralNode*>(expr))
	{
		(void)stringLiteral;
		return;
	}

	if (const auto* numberLiteral = dynamic_cast<const NumberNode*>(expr))
	{
		(void)numberLiteral;
		return;
	}
}

void Optimizer::collectUnusedDecls(const std::vector<std::unique_ptr<StmtNode>>& statements,
								   const SymbolUsageMap& usage,
								   std::unordered_set<SymbolID>& unused) const
{
	for (const auto& stmt : statements)
	{
		if (!stmt)
			continue;

		if (const auto* decl = dynamic_cast<const DeclNode*>(stmt.get()))
		{
			const SymbolID symbol = decl->symbolId();
			if (symbol != InvalidSymbolID && usage.find(symbol) == usage.end())
				unused.insert(symbol);
			continue;
		}

		if (const auto* fn = dynamic_cast<const FunctionNode*>(stmt.get()))
		{
			collectUnusedDecls(fn->body(), usage, unused);
			continue;
		}

		if (const auto* ifNode = dynamic_cast<const IfNode*>(stmt.get()))
		{
			collectUnusedDecls(ifNode->thenBlock(), usage, unused);
			collectUnusedDecls(ifNode->elseBlock(), usage, unused);
			continue;
		}

		if (const auto* structDecl = dynamic_cast<const StructDeclNode*>(stmt.get()))
		{
			for (const auto& method : structDecl->functions())
			{
				if (method)
					collectUnusedDecls(method->body(), usage, unused);
			}
			continue;
		}
	}
}

void Optimizer::collectUnusedDecls(const BlockNode* block,
								   const SymbolUsageMap& usage,
								   std::unordered_set<SymbolID>& unused) const
{
	if (!block)
		return;

	collectUnusedDecls(block->statements(), usage, unused);
}

bool Optimizer::pruneStatements(std::vector<std::unique_ptr<StmtNode>>& statements,
								const std::unordered_set<SymbolID>& unused) const
{
	bool changed = false;

	for (auto it = statements.begin(); it != statements.end();)
	{
		StmtNode* stmt = it->get();
		if (!stmt)
		{
			it = statements.erase(it);
			changed = true;
			continue;
		}

		if (auto* fn = dynamic_cast<FunctionNode*>(stmt))
		{
			if (auto* body = fn->body())
				changed |= pruneStatements(body->statements(), unused);
		}
		else if (auto* ifNode = dynamic_cast<IfNode*>(stmt))
		{
			if (auto* thenBlock = ifNode->thenBlock())
				changed |= pruneStatements(thenBlock->statements(), unused);
			if (auto* elseBlock = ifNode->elseBlock())
				changed |= pruneStatements(elseBlock->statements(), unused);
		}
		else if (auto* structDecl = dynamic_cast<StructDeclNode*>(stmt))
		{
			for (auto& method : structDecl->functions())
			{
				if (method && method->body())
					changed |= pruneStatements(method->body()->statements(), unused);
			}
		}

		if (pruneStatement(*stmt, unused))
		{
			it = statements.erase(it);
			changed = true;
			continue;
		}

		++it;
	}

	return changed;
}

bool Optimizer::pruneStatement(StmtNode& stmt, const std::unordered_set<SymbolID>& unused) const
{
	if (auto* decl = dynamic_cast<DeclNode*>(&stmt))
	{
		const SymbolID symbol = decl->symbolId();
		return symbol != InvalidSymbolID && unused.find(symbol) != unused.end();
	}

	if (auto* assign = dynamic_cast<AssignNode*>(&stmt))
	{
		const SymbolID symbol = assign->symbolId();
		return symbol != InvalidSymbolID && unused.find(symbol) != unused.end();
	}

	return false;
}
