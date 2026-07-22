# openscad_cpp_parser

A C++20 parser for the [OpenSCAD](https://openscad.org/) language: lexer + grammar, an AST,
lexical scope resolution, comment-preserving pretty-printing, and JSON/YAML serialization.
It's a from-scratch C++ port of the `openscad_lalr_parser` Python reference implementation,
built to be usable both as a library and as a standalone command-line tool.

## Building

Requirements:

- CMake ≥ 3.24
- A C++20 compiler
- Bison ≥ 3.5 (needs `%skeleton "lalr1.cc"` + variant support — see note below)
- Flex

`nlohmann_json` and `googletest` are downloaded automatically via CMake's `FetchContent`; you
don't need to install them yourself.

```bash
cmake -S . -B build
cmake --build build -j
```

**macOS note:** the system-provided `bison` (2.3, GPLv2) is too old. Install a modern one and
CMake will find it automatically:

```bash
brew install bison
```

(`CMakeLists.txt` looks for `/opt/homebrew/opt/bison` or `/usr/local/opt/bison` before falling
back to whatever `bison` is on `PATH`.)

### Running the tests

```bash
ctest --test-dir build --output-on-failure
```

## Command-line tool

The build produces `build/tools/cli/openscad-cpp-parser`, which parses an OpenSCAD file (or
stdin) and prints its AST or a reformatted version of the source.

```
usage: openscad-cpp-parser [-j|-y|-r] [-c|-C] [-i] [--indent N] [FILE]

Parse an OpenSCAD file and dump its AST as JSON (default), YAML, or
reformatted OpenSCAD source. Omit FILE or pass '-' to read from stdin.

  -j, --json          Output AST as JSON (default).
  -y, --yaml          Output AST as YAML.
  -r, --format        Output reformatted OpenSCAD source (comments preserved).
  -c, --no-comments   Exclude comments from the output.
  -C, --with-comments Include comments in the output.
  -i, --no-includes   Do not expand include <...> statements.
      --indent N      Indentation width in spaces (default: 4).
```

Examples:

```bash
# Dump a file's AST as JSON
build/tools/cli/openscad-cpp-parser -j model.scad

# Reformat source read from stdin, preserving comments
echo 'module foo(a,b=2){cube(a);}' | build/tools/cli/openscad-cpp-parser -r
# module foo(a, b=2) {
#     cube(a);
# }

# Dump as YAML instead
echo 'x = 1 + 2;' | build/tools/cli/openscad-cpp-parser -y
```

On a syntax error, the tool prints a caret-pointing diagnostic to stderr and exits non-zero:

```
$ echo 'x = ;' | build/tools/cli/openscad-cpp-parser
openscad-cpp-parser: Syntax error in <string> at line 1, column 5:
x = ;
    ^
syntax error, unexpected ;
```

## Using it as a library

Add the project via `add_subdirectory()` (or `FetchContent`) and link against the
`openscad_cpp_parser` target:

```cmake
add_subdirectory(openscad_cpp_parser)
target_link_libraries(your_target PRIVATE openscad_cpp_parser)
```

The public API lives in `openscad_cpp_parser/api.hpp`:

```cpp
#include "openscad_cpp_parser/api.hpp"
#include "openscad_cpp_parser/pretty_print.hpp"
#include "openscad_cpp_parser/serialization.hpp"

using namespace oscad;

try {
    // Parse a string. Pass includeComments=true to keep comments/blank lines
    // in the tree (as CommentLine/BlankLine/CommentedExpr nodes).
    auto ast = getASTFromString("cube([1, 2, 3]);\n", /*includeComments=*/true);

    // Or parse a file, splicing in `include <...>` targets automatically:
    auto fileAst = getASTFromFile("model.scad");

    // Resolve variable/function/module scoping over the tree.
    auto root = buildScopes(ast);

    // Serialize.
    std::string json = astToJsonString(ast);
    std::string yaml = astToYamlString(ast);
    std::string src  = toOpenscad(ast);
} catch (const ParseError& e) {
    // e.what() is the same caret-pointing diagnostic the CLI prints.
    std::cerr << e.what() << "\n";
}
```

Each node type (`Assignment`, `ModularCall`, `FunctionDeclaration`, ...) is declared in
`openscad_cpp_parser/ast/*.hpp` and reachable via `dynamic_cast` from the `ASTNode*`/`Expression*`
base pointers held in the returned vectors, e.g.:

```cpp
auto ast = parseAst("x = 42;");
auto* assign = dynamic_cast<Assignment*>(ast[0].get());
auto* num = dynamic_cast<NumberLiteral*>(assign->expr.get());
num->val; // 42.0
```

See `tests/` for further usage examples covering expressions, control flow, modules,
functions, comments, scope resolution, and serialization.
