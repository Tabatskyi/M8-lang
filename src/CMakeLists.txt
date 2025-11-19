# CMakeList.txt : CMake project for M8-lang, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (M8-lang "Compiler/M8-lang.cpp" "Compiler/M8-lang.hpp" "SyntaxParser/SyntaxParser.hpp" "SyntaxParser/SyntaxParser.cpp" "General/Token.hpp" "AST/" "Semantics/Semantics.hpp" "Semantics/Semantics.cpp" "CodeGen/CodeGen.hpp" "CodeGen/CodeGen.cpp" "Lexer/Lexer.hpp" "Lexer/Lexer.cpp")

# Enforce C++23 via compile features to ensure MSVC adds /std:c++23
target_compile_features(M8-lang PUBLIC cxx_std_23)