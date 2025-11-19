#include "DeclNode.hpp"

#include <utility>

#include "ASTVisitor.hpp"

DeclNode::DeclNode(TypeDesc type, std::string identifier, bool isMutable, std::vector<std::unique_ptr<ExprNode>> initializers)
    : _declType(std::move(type)), _identifier(std::move(identifier)), _isMutable(isMutable), _initializers(std::move(initializers)) {}

const TypeDesc& DeclNode::declaredType() const
{
    return _declType;
}

const std::string& DeclNode::identifier() const
{
    return _identifier;
}

bool DeclNode::isMutable() const
{
    return _isMutable;
}

bool DeclNode::hasInitializer() const
{
    return !_initializers.empty();
}

const std::vector<std::unique_ptr<ExprNode>>& DeclNode::initializers() const
{
    return _initializers;
}

SymbolID DeclNode::symbolId() const
{
    return _symbolId;
}

void DeclNode::setSymbolId(SymbolID id) const
{
    _symbolId = id;
}

void DeclNode::accept(ASTVisitor& visitor) const
{
    visitor.visitDecl(*this);
}
