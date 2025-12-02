# CMakeList.txt : CMake project for M8-lang, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (M8-lang 
    "Compiler/M8-lang.cpp" 
    "Compiler/M8-lang.hpp" 
    "Compiler/Optimizer.cpp"
    "Compiler/Optimizer.hpp"
    "SyntaxParser/SyntaxParser.hpp" 
    "SyntaxParser/SyntaxParser.cpp" 
    "General/Token.hpp" 
    "Semantics/Semantics.hpp" 
    "Semantics/Semantics.cpp" 
    "CodeGen/CodeGen.hpp" 
    "CodeGen/CodeGen.cpp" 
    "Lexer/Lexer.hpp" 
    "Lexer/Lexer.cpp"
    "AST/ASTNode.cpp"
    "AST/AssignFieldNode.cpp"
    "AST/AssignNode.cpp"
    "AST/ASTCloner.cpp"
    "AST/BinaryOpNode.cpp"
    "AST/BlockNode.cpp"
    "AST/BoolLiteralNode.cpp"
    "AST/DeclNode.cpp"
    "AST/ExprNode.cpp"
    "AST/ExprStmtNode.cpp"
    "AST/FactorNode.cpp"
    "AST/FieldAccessNode.cpp"
    "AST/FunctionCallNode.cpp"
    "AST/FunctionNode.cpp"
    "AST/IDNode.cpp"
    "AST/IfNode.cpp"
    "AST/MemberFunctionCallNode.cpp"
    "AST/NumberNode.cpp"
    "AST/ProgramNode.cpp"
    "AST/ReturnNode.cpp"
    "AST/StringLiteralNode.cpp"
    "AST/StmtNode.cpp"
    "AST/StructDeclNode.cpp"
    "AST/StructLiteralNode.cpp"
    "AST/UnaryOpNode.cpp"
)

# Enforce C++23 via compile features to ensure MSVC adds /std:c++23
target_compile_features(M8-lang PUBLIC cxx_std_23)