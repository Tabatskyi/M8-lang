#pragma once

#include "ASTVisitor.hpp"
#include "FactorNode.hpp"

#include <string>

class StringLiteralNode : public FactorNode
{
public:
    explicit StringLiteralNode(std::string value);

    const std::string& value() const;

    void accept(ASTVisitor& visitor) const override;

private:
    std::string _value;
};