# CMakeList.txt : CMake project for M8-lang, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (M8-lang "M8-lang.cpp" "M8-lang.hpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET M8-lang PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
