#pragma once

class ASTVisitor;

class ASTNode
{
public:
	virtual ~ASTNode() = default;

	size_t line() const;
	void setLine(size_t line) const;

	virtual void accept(ASTVisitor& visitor) const = 0;

protected:
	ASTNode() = default;
	ASTNode(const ASTNode&) = default;
	ASTNode& operator=(const ASTNode&) = default;

private:
	mutable size_t _line = 0;
};