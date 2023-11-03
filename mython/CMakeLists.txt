cmake_minimum_required(VERSION 3.10)
project(mython)
set(CMAKE_CXX_STANDARD 17)

add_compile_options(-Wall -Werror -Wunused -Wextra -Wpedantic)

set(PARSER parse.h parse.cpp)
set(AST statement.h statement.cpp)
set(RUNTIME runtime.h runtime.cpp)
set(LEXER lexer.h lexer.cpp)
set(TEST_FILES statement_test.cpp parse_test.cpp runtime_test.cpp lexer_test_open.cpp test_runner.h)

add_executable(mython main.cpp ${PARSER} ${AST} ${RUNTIME} ${LEXER} ${TEST_FILES})
