# CMakeList.txt : CMake project for M8-lang, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (M8-lang "M8-lang.cpp" "M8-lang.hpp" "SyntaxParser.hpp" "SyntaxParser.cpp" "Token.hpp" "ASTNode.hpp")

# Enforce C++20 via compile features to ensure MSVC adds /std:c++20
target_compile_features(M8-lang PUBLIC cxx_std_20)

# TODO: Add tests and install targets if needed.
