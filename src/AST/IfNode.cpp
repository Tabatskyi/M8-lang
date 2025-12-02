#include "IfNode.hpp"

IfNode::IfNode(std::unique_ptr<ExprNode> condition, std::unique_ptr<BlockNode> thenBlock, std::unique_ptr<BlockNode> elseBlock)
    : _condition(std::move(condition)), _thenBlock(std::move(thenBlock)), _elseBlock(std::move(elseBlock)) {}

const ExprNode* IfNode::condition() const
{
    return _condition.get();
}

ExprNode* IfNode::condition()
{
    return _condition.get();
}

void IfNode::setCondition(std::unique_ptr<ExprNode> condition)
{
    _condition = std::move(condition);
}

const BlockNode* IfNode::thenBlock() const
{
    return _thenBlock.get();
}

BlockNode* IfNode::thenBlock()
{
    return _thenBlock.get();
}

void IfNode::setThenBlock(std::unique_ptr<BlockNode> thenBlock)
{
    _thenBlock = std::move(thenBlock);
}

const BlockNode* IfNode::elseBlock() const
{
    return _elseBlock.get();
}

BlockNode* IfNode::elseBlock()
{
    return _elseBlock.get();
}

void IfNode::setElseBlock(std::unique_ptr<BlockNode> elseBlock)
{
    _elseBlock = std::move(elseBlock);
}

void IfNode::accept(ASTVisitor& visitor) const
{
    visitor.visitIf(*this);
}
