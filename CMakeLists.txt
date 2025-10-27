cmake_minimum_required (VERSION 3.10)

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("M8-lang")

if (DEFINED CMAKE_CXX_COMPILER_FRONTEND_VARIANT AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
  add_compile_options(/utf-8)
elseif (MSVC)
  add_compile_options(/utf-8)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-finput-charset=UTF-8)
endif()

add_subdirectory ("src")
