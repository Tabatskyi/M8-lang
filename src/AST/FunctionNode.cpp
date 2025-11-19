#include "FunctionNode.hpp"

#include <utility>

#include "ASTVisitor.hpp"

FunctionNode::FunctionNode(std::string name, std::vector<Param> params, TypeDesc returnType, std::unique_ptr<BlockNode> body, size_t scopeId, std::string masterStruct)
    : _name(std::move(name)), _params(std::move(params)), _returnType(std::move(returnType)), _body(std::move(body)), _scopeId(scopeId), _masterStruct(std::move(masterStruct)) {}

const std::string& FunctionNode::name() const
{
    return _name;
}

const std::vector<FunctionNode::Param>& FunctionNode::params() const
{
    return _params;
}

const TypeDesc& FunctionNode::returnType() const
{
    return _returnType;
}

const BlockNode* FunctionNode::body() const
{
    return _body.get();
}

size_t FunctionNode::scopeId() const
{
    return _scopeId;
}

bool FunctionNode::isMember() const
{
    return !_masterStruct.empty();
}

const std::string& FunctionNode::masterStruct() const
{
    return _masterStruct;
}

void FunctionNode::accept(ASTVisitor& visitor) const
{
    visitor.visitFunction(*this);
}
