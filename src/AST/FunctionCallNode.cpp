#include "FunctionCallNode.hpp"

#include <utility>

#include "ASTVisitor.hpp"

FunctionCallNode::FunctionCallNode(std::string name, std::vector<std::unique_ptr<ExprNode>> arguments)
    : _name(std::move(name)), _args(std::move(arguments)) {}

const std::string& FunctionCallNode::name() const
{
    return _name;
}

const std::vector<std::unique_ptr<ExprNode>>& FunctionCallNode::args() const
{
    return _args;
}

void FunctionCallNode::accept(ASTVisitor& visitor) const
{
    visitor.visitFunctionCall(*this);
}
