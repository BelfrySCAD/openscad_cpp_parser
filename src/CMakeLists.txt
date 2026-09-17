bison_target(OscadParser
  grammar/parser.y
  ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.cpp
  DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.hpp
)
flex_target(OscadLexer
  grammar/lexer.l
  ${CMAKE_CURRENT_BINARY_DIR}/lexer.yy.cpp
)
add_flex_bison_dependency(OscadLexer OscadParser)

add_library(openscad_cpp_parser STATIC
  ${BISON_OscadParser_OUTPUTS}
  ${FLEX_OscadLexer_OUTPUTS}
  grammar/driver.cpp
  ast/ast_node.cpp
  ast/expression.cpp
  ast/vector_element.cpp
  ast/module_instantiation.cpp
  ast/declarations.cpp
  ast/scope_builder.cpp
  position.cpp
  scope.cpp
  api.cpp
  pretty_print.cpp
  comments.cpp
  inline_comment_attach.cpp
  source_map.cpp
  serialization/json_io.cpp
  serialization/yaml_io.cpp
)

target_include_directories(openscad_cpp_parser PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)
target_include_directories(openscad_cpp_parser PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/grammar
  ${CMAKE_CURRENT_BINARY_DIR}
)
# CMAKE_DL_LIBS is -ldl for api.cpp's dladdr() -- empty on macOS and
# Windows, where it lives in libSystem/kernel32. A local macOS build
# therefore links fine and manylinux (glibc < 2.34, dl still a separate
# library) does not: caught only by a wheel build.
target_link_libraries(openscad_cpp_parser PUBLIC nlohmann_json::nlohmann_json ${CMAKE_DL_LIBS})
# SHGetFolderPathW, for the real My Documents rather than an assumed
# %USERPROFILE%\Documents -- see api.cpp userLibraryDir().
if(WIN32)
  target_link_libraries(openscad_cpp_parser PUBLIC shell32)
endif()
target_compile_options(openscad_cpp_parser PRIVATE
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall;-Wextra>
)
