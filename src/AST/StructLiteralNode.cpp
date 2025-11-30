#include "StructLiteralNode.hpp"

StructLiteralNode::StructLiteralNode(TypeDesc type, std::vector<std::unique_ptr<ExprNode>> args)
    : _structType(std::move(type)), _args(std::move(args)) {}

const TypeDesc& StructLiteralNode::structType() const
{
    return _structType;
}

const std::vector<std::unique_ptr<ExprNode>>& StructLiteralNode::args() const
{
    return _args;
}

void StructLiteralNode::accept(ASTVisitor& visitor) const
{
    visitor.visitStructLiteral(*this);
}
