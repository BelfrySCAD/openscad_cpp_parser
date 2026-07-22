# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A C++20 port of a Python OpenSCAD parser (`openscad_lalr_parser` — a Bison/Flex-based
lexer+grammar with an AST, scope resolution, comment attachment, and JSON/YAML
serialization). Comments throughout the code reference the Python reference implementation
by module name (`nodes.py`, `scope.py`, `source_map.py`, `serialization.py`,
`__init__.py`'s `_extract_comments`/`_attach_inline_comments`/etc.). **When changing behavior,
check whether it's meant to mirror the Python reference's behavior exactly** — many design
choices here (error message format, scope shadowing rules, comment-attachment fallbacks) are
deliberate ports, not independent designs. Divergences from the reference are called out
explicitly in comments (grep for "mirrors" and "unlike the Python reference").

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

Requires Bison ≥3.5 (with `%skeleton "lalr1.cc"` + variant support — macOS's
system bison is too old; CMakeLists.txt auto-detects a Homebrew bison at
`/opt/homebrew/opt/bison` or `/usr/local/opt/bison`) and Flex. `nlohmann_json`
and `googletest` are fetched automatically via `FetchContent`.

## Test

```bash
ctest --test-dir build                      # all tests
build/tests/oscad_tests                     # run the gtest binary directly
build/tests/oscad_tests --gtest_filter='Smoke.*'      # one suite
build/tests/oscad_tests --gtest_filter='Smoke.DanglingElseAttachesToInnerIf'  # one test
```

Tests are plain GoogleTest (`TEST(Suite, Case)`), one file per feature area under `tests/`
(assignments, control flow, expressions, vectors, modules, comments, scope, serialization,
pretty-print, source maps, ...). New test files must be added to `tests/CMakeLists.txt`'s
`add_executable(oscad_tests ...)` list.

## CLI tool

`build/tools/cli/openscad-cpp-parser` (see `tools/cli/main.cpp`) parses a file (or stdin) and
dumps JSON (default), YAML (`-y`), or reformatted OpenSCAD source (`-r`). Useful for manually
sanity-checking a grammar/AST change end-to-end: `echo 'x = 1 + 2;' | build/tools/cli/openscad-cpp-parser -r`.

## Architecture

**Pipeline:** `lexer.l` (Flex) → `parser.y` (Bison, LALR1 C++ skeleton, variant-typed
semantic values) → `ParserDriver` (`src/grammar/driver.hpp/cpp`) builds the AST via
`make*()` node-construction helpers → `getASTFromString`/`getASTFromFile` (`api.hpp`) is the
public entry point, optionally splicing `include <...>` files together first via `SourceMap`.

- **Grammar/lexer** (`src/grammar/`): `parser.y` builds nodes only through the `make*()`
  helpers declared in `driver.hpp` — never construct AST node types directly in grammar
  actions. Locations use a custom flat `OscadLocation` (not Bison's default begin/end position
  pair) — see `YYLLOC_DEFAULT` override in `parser.y` and `oscad_location.hpp`.
- **AST** (`include/openscad_cpp_parser/ast/`, `src/ast/`): every node derives from `ASTNode`
  (`ast_node.hpp`) with a `NodeKind` enum tag (one value per *leaf* class only — abstract
  bases like `Expression`/`Primary`/`Argument` are never instantiated directly), a `Position`,
  and a `Scope*` populated later by `buildScope()`. There is no reflection in C++, so anything
  that must visit "all fields of all node kinds" (JSON I/O, pretty-printing, comment
  attachment) is a hand-written switch/dispatch per kind, not a generic walker — expect to
  touch several such dispatch sites when adding a new node kind (`serialization/json_io.cpp`,
  `pretty_print.cpp`, `inline_comment_attach.cpp`'s `classifyNode()`, `scope_builder.cpp`).
- **Scope** (`scope.hpp`, `src/ast/scope_builder.cpp`): three independent namespaces
  (variables/functions/modules) per `Scope`, parent-chain lookup, last-write-wins (no
  shadowing diagnostics), matching the Python reference exactly. `Scope` owns its child scopes;
  `ASTNode::scope()` is a non-owning pointer into that tree, valid as long as the root `Scope`
  from `buildScopes()` is alive.
- **Comments** (`comments.cpp`, `inline_comment_attach.cpp`): comments are stripped by the
  lexer during the main parse, then re-extracted from the raw source text in a second pass and
  spliced back in — standalone comments/blank lines as top-level nodes, inline comments
  wrapped as `CommentedExpr` around the nearest expression (falling back to the nearest
  preceding node's last expression if no exact match). This is intentionally *not* a 1:1 port
  of the reference's generic-reflection walk; see the comment at the top of `comments.hpp` for
  which parts deliberately diverge and why.
- **SourceMap** (`source_map.hpp/cpp`): splices multiple origins (files) into one combined
  buffer for a single parse pass (needed for `include <...>`), and maps combined-buffer offsets
  back to the original (origin, line, column) for diagnostics.
- **Serialization** (`serialization.hpp`, `src/serialization/`): JSON schema
  (`{"_type": "ClassName", "_position": {...}?, <field>: <value>}`) matches the Python
  reference's field names (snake_case in JSON) independent of this port's camelCase C++ member
  names, so output from either implementation is structurally comparable. YAML support is a
  hand-rolled restricted emitter/parser covering only the shape JSON serialization produces —
  not a general YAML library.
- **Error handling**: `parseAst`/`getASTFromString`/`getASTFromFile` all throw `ParseError`
  (carrying a caret-pointing diagnostic string) on syntax errors — this is a deliberate
  divergence from the Python reference's `None`-on-failure return (see the note in `api.hpp`);
  printing/swallowing the error is the CLI's job, not the library's.
- **No AST caching**: unlike the Python reference, `getASTFromFile` re-parses every call (no
  clone() exists for the 66 node kinds to support a cache); see the `ponytail:` note in
  `api.hpp` for the upgrade path if this ever matters.

## Conventions in this codebase

- Comments are used deliberately and mean something — they explain *why*, note deviations from
  the Python reference, or mark a `ponytail:`-tagged known simplification with its upgrade
  path. Read them before changing the code they annotate; preserve/update them when you do.
- Constness is used to enforce actual invariants (e.g. `ASTNode::scope()`/`Scope::parent()`
  are overloaded on `this`'s constness specifically so a `const` borrow can't reach through to
  a mutating call) — don't casually add a non-const accessor "for convenience."
