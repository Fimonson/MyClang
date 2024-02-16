message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "LLVM include dir is ${LLVM_INCLUDE_DIR}")

add_shad_executable(check_names SOLUTION_SRCS check_names.cpp)

target_include_directories(check_names SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(check_names PRIVATE ${LLVM_DEFINITIONS})
target_link_directories(check_names PRIVATE ${LLVM_LIBRARY_DIRS})

target_link_libraries(check_names
  clangAnalysis
  clangAST
  clangASTMatchers
  clangBasic
  clangCodeGen
  clangDriver
  clangEdit
  clangFrontend
  clangLex
  clangParse
  clangRewrite
  clangSema
  clangSerialization
  clangTooling)

set(TESTS_LIST
  tests/no-dict/fun.cpp
  tests/no-dict/perm.cpp
  tests/no-dict/segment.cpp
  tests/no-dict/lib.cpp
  tests/dict/test_file.cpp
  tests/dict/sorting.cpp)

foreach (test ${TESTS_LIST})
  get_filename_component(result ${test} NAME_WE)
  set(TARGET "clang_test_${result}")
  add_executable(${TARGET} ${test})
  target_compile_options(${TARGET} PRIVATE "-w")
endforeach(test)

if (GRADER)
  set(QUIET "-q")
endif()

add_custom_target(
  test_check_names
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  DEPENDS check_names
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_tests.sh ${CMAKE_CURRENT_SOURCE_DIR} ${QUIET}
  VERBATIM)
