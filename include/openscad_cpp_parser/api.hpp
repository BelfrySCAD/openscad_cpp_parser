#pragma once

#include "openscad_cpp_parser/ast.hpp"
#include "openscad_cpp_parser/source_map.hpp"

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace oscad {

// Thrown by parseAst() (and anything that calls it) on a syntax error.
// Carries a caret-pointing diagnostic, mirroring the text the Python
// reference prints to stdout on parse failure -- but as an exception, since
// "print and return None" is CLI behavior, not library behavior. The CLI
// tool is what prints `what()`.
class ParseError : public std::runtime_error {
public:
    explicit ParseError(const std::string& message) : std::runtime_error(message) {}
};

// Low-level: parses `code` with no caching and no include-splicing. Throws
// ParseError on a syntax error; the message is a caret-pointing diagnostic
// ("Syntax error in {origin} at line L, column C:\n{line text}\n{caret}"),
// mirroring what the Python reference prints to stdout on parse failure --
// but returned as an exception message, since printing is CLI behavior,
// not library behavior (the CLI tool prints ParseError::what()).
//
// If `sourceMap` is given (typically because `code` is
// sourceMap->getCombinedString() after processIncludes() spliced several
// files together), the error position is translated back through the
// source map so the diagnostic names the correct original file/line/column
// instead of a position in the combined buffer.
std::vector<std::unique_ptr<ASTNode>> parseAst(const std::string& code, const std::string& origin = "<string>",
                                                SourceMap* sourceMap = nullptr);

// Builds the lexical-scope tree for a parsed AST: hoists top-level
// Assignment/FunctionDeclaration/ModuleDeclaration nodes into a root Scope,
// then calls buildScope() on every top-level node so every node in the tree
// gets its scope() populated. Mirrors scope.py's build_scopes().
std::unique_ptr<Scope> buildScopes(const std::vector<std::unique_ptr<ASTNode>>& ast);

// Same, but for nodes referenced by raw pointer -- see
// collectHoistedDeclarations's raw-pointer overload (scope_builder.hpp) for
// why this exists. Never takes ownership.
std::unique_ptr<Scope> buildScopes(const std::vector<ASTNode*>& ast);

// Parses `code`. Throws ParseError (with the full caret diagnostic) on a
// syntax error.
//
// NOTE: this does NOT mirror Python's `None`-on-failure return for
// getASTfromString. An earlier version of this port did (returning
// std::optional and swallowing ParseError into nullopt here), but that
// throws away the diagnostic message before any caller -- including the
// CLI -- ever sees it: nullopt carries no information about what went
// wrong. Throwing uniformly (parseAst and everything built on it) means
// there is exactly one error-reporting path, and it's always the rich one.
std::vector<std::unique_ptr<ASTNode>> getASTFromString(const std::string& code, bool includeComments = false,
                                                        const std::string& origin = "<string>");

// Parses a file, resolving `include <...>` statements by splicing in the
// referenced file's AST (recursively, cycle-safe) unless processIncludes is
// false. `use <...>` is left as an unresolved UseStatement node (matches
// the reference: it only affects the symbol table, not statement-level
// code). Throws std::runtime_error if the file (or an include target)
// can't be found/read, or ParseError (see getASTFromString) on a syntax
// error in any of the involved files.
//
// ponytail: unlike the Python reference, this does NOT cache parsed ASTs
// (in-memory or on-disk) across calls. With unique_ptr ownership, "return
// the same cached tree to every caller" isn't representable (each caller
// needs exclusive ownership), and a clone-based cache would need a deep
// clone() for all 66 node kinds just to support an unmeasured perf
// optimization. Every call re-parses. Upgrade path: add a per-node
// clone() (mirroring toJson/toString's per-kind dispatch) and cache
// serialized snapshots keyed by (path, mtime) if repeated-parse cost of a
// real workload is shown to matter.
std::vector<std::unique_ptr<ASTNode>> getASTFromFile(const std::string& file, bool includeComments = false,
                                                      bool processIncludes = true);

struct LibraryFileResult {
    std::vector<std::unique_ptr<ASTNode>> ast;
    std::string resolvedPath;
};

// Finds `libFile` via findLibraryFile() then parses it with getASTFromFile().
// Throws std::runtime_error if the library file cannot be found.
LibraryFileResult getASTFromLibraryFile(const std::string& currFile, const std::string& libFile,
                                         bool includeComments = false, bool processIncludes = true);

// OpenSCAD's library search path: (1) directory of currFile, (2)
// OPENSCADPATH env var (':'-separated on POSIX, ';' on Windows), (3)
// platform default library dir.
std::optional<std::string> findLibraryFile(const std::string& currFile, const std::string& libFile);

// No-op: kept for API-shape parity with the Python reference's
// clear_ast_cache(). See the ponytail note on getASTFromFile() -- this
// port doesn't cache, so there's nothing to clear.
void clearAstCache();

} // namespace oscad
