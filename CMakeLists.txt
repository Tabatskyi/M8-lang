cmake_minimum_required (VERSION 3.10)

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# Single project declaration; restrict to C++ only
project ("M8-lang" LANGUAGES CXX)

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

# -----------------------------
# Testing (CTest)
# -----------------------------
include(CTest)
if (BUILD_TESTING)
  enable_testing()

  # Directory to write compiler outputs during tests
  set(TEST_OUTPUT_DIR "${CMAKE_BINARY_DIR}/test_outputs")
  file(MAKE_DIRECTORY "${TEST_OUTPUT_DIR}")

  # Gather test files
  file(GLOB PASS_TESTS "${CMAKE_CURRENT_SOURCE_DIR}/test/pass/*.m8")
  file(GLOB FAIL_TESTS "${CMAKE_CURRENT_SOURCE_DIR}/test/fail/*.m8")

  # Add passing tests: expect exit code 0
  foreach(TEST_FILE IN LISTS PASS_TESTS)
    get_filename_component(TEST_NAME_WE "${TEST_FILE}" NAME_WE)
    add_test(
      NAME pass_${TEST_NAME_WE}
      COMMAND $<TARGET_FILE:M8-lang> "${TEST_FILE}" "${TEST_OUTPUT_DIR}/${TEST_NAME_WE}.ll"
    )
  endforeach()

  # Add failing tests: expect non-zero exit code
  foreach(TEST_FILE IN LISTS FAIL_TESTS)
    get_filename_component(TEST_NAME_WE "${TEST_FILE}" NAME_WE)
    add_test(
      NAME fail_${TEST_NAME_WE}
      COMMAND $<TARGET_FILE:M8-lang> "${TEST_FILE}" "${TEST_OUTPUT_DIR}/${TEST_NAME_WE}.ll"
    )
    set_tests_properties(fail_${TEST_NAME_WE} PROPERTIES WILL_FAIL TRUE)
  endforeach()
endif()
