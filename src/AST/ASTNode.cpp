#include "ASTNode.hpp"

size_t ASTNode::line() const
{
    return _line;
}

void ASTNode::setLine(size_t line) const
{
    _line = line;
}