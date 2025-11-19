#include "MemberFunctionCallNode.hpp"

#include <utility>

#include "ASTVisitor.hpp"

MemberFunctionCallNode::MemberFunctionCallNode(std::string base, std::vector<std::string> chain, std::string function, std::vector<std::unique_ptr<ExprNode>> arguments)
            : _base(std::move(base)), _fields(std::move(chain)), _funcName(std::move(function)), _args(std::move(arguments)) {}

const std::string& MemberFunctionCallNode::base() const
{
    return _base;
}

const std::vector<std::string>& MemberFunctionCallNode::fieldChain() const
{
    return _fields;
}

const std::string& MemberFunctionCallNode::funcName() const
{
    return _funcName;
}

const std::vector<std::unique_ptr<ExprNode>>& MemberFunctionCallNode::args() const
{
    return _args;
}

SymbolID MemberFunctionCallNode::baseSymbolId() const
{
    return _baseId;
}

void MemberFunctionCallNode::setBaseSymbolId(SymbolID id) const
{
    _baseId = id;
}

void MemberFunctionCallNode::accept(ASTVisitor& visitor) const
{
    visitor.visitMemberFunctionCall(*this);
}
