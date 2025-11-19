#include "StructDecNode.hpp"

#include <utility>

#include "ASTVisitor.hpp"

StructDeclNode::StructDeclNode(std::string name,
                               std::vector<Field> fields,
                               std::vector<std::unique_ptr<FunctionNode>> methods)
    : _name(std::move(name)),
      _fields(std::move(fields)),
      _functions(std::move(methods))
{
}

const std::string& StructDeclNode::name() const
{
    return _name;
}

const std::vector<StructDeclNode::Field>& StructDeclNode::fields() const
{
    return _fields;
}

const std::vector<std::unique_ptr<FunctionNode>>& StructDeclNode::functions() const
{
    return _functions;
}

void StructDeclNode::accept(ASTVisitor& visitor) const
{
    visitor.visitStructDecl(*this);
}