FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(oscad_tests
  test_smoke.cpp
  test_pretty_print.cpp
  test_serialization.cpp
  test_comments_and_files.cpp
  test_inline_comments.cpp
  test_source_map.cpp
  test_const_borrow.cpp
  test_lexical.cpp
  test_assignments.cpp
  test_functions.cpp
  test_use_include.cpp
  test_expressions.cpp
  test_vectors.cpp
  test_modules.cpp
  test_control.cpp
  test_scope.cpp
  test_ast_generation.cpp
  test_node_str.cpp
  test_render_expression.cpp
)
target_link_libraries(oscad_tests PRIVATE openscad_cpp_parser GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(oscad_tests)
