#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ExprNode.hpp"
#include "TypeDesc.hpp"

class StructLiteralNode : public ExprNode
{
public:
    StructLiteralNode(TypeDesc type, std::vector<std::unique_ptr<ExprNode>> args);

    const TypeDesc& structType() const;
    const std::vector<std::unique_ptr<ExprNode>>& args() const;

    void accept(ASTVisitor& visitor) const override;

private:
    TypeDesc _structType;
    std::vector<std::unique_ptr<ExprNode>> _args;
};
