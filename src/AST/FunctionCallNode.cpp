#include "FunctionCallNode.hpp"

FunctionCallNode::FunctionCallNode(std::string name, std::vector<std::unique_ptr<ExprNode>> arguments)
    : _name(std::move(name)), _arguments(std::move(arguments)) {}

const std::string& FunctionCallNode::name() const
{
    return _name;
}

const std::vector<std::unique_ptr<ExprNode>>& FunctionCallNode::args() const
{
    return _arguments;
}

void FunctionCallNode::setSymbolId(SymbolID id) const
{
    _symbolId = id;
}

SymbolID FunctionCallNode::symbolId() const
{
    return _symbolId;
}

void FunctionCallNode::accept(ASTVisitor& visitor) const
{
    visitor.visitFunctionCall(*this);
}
