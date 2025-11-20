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
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  find_program(LLVM_LLC_EXECUTABLE NAMES llc)
  find_program(LLVM_CLANG_EXECUTABLE NAMES clang)

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

  if (LLVM_LLC_EXECUTABLE AND LLVM_CLANG_EXECUTABLE)
    file(GLOB E2E_TESTS "${CMAKE_CURRENT_SOURCE_DIR}/test/e2e/*.m8")
    foreach(TEST_FILE IN LISTS E2E_TESTS)
      get_filename_component(TEST_NAME_WE "${TEST_FILE}" NAME_WE)
      set(TEST_NAME e2e_${TEST_NAME_WE})
      set(EXPECT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/test/e2e/${TEST_NAME_WE}.out")
      if (NOT EXISTS "${EXPECT_FILE}")
        message(FATAL_ERROR "Missing expected output file for ${TEST_NAME}")
      endif()
      set(TEST_COMMAND
        ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_e2e_test.py"
        --compiler $<TARGET_FILE:M8-lang>
        --source "${TEST_FILE}"
        --work-dir "${TEST_OUTPUT_DIR}"
        --test-name ${TEST_NAME}
        --expected "${EXPECT_FILE}"
        --llc "${LLVM_LLC_EXECUTABLE}"
        --clang "${LLVM_CLANG_EXECUTABLE}"
      )
      set(INPUT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/test/e2e/${TEST_NAME_WE}.in")
      if (EXISTS "${INPUT_FILE}")
        list(APPEND TEST_COMMAND --input "${INPUT_FILE}")
      endif()
      add_test(NAME ${TEST_NAME} COMMAND ${TEST_COMMAND})
    endforeach()
  else()
    message(WARNING "Skipping end-to-end tests because llc/clang were not found on PATH.")
  endif()
endif()
