#pragma once

#include "openscad_cpp_parser/ast.hpp"
#include "openscad_cpp_parser/scope_table.hpp"
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

// Same, but recording into a table the CALLER owns, and not attaching it to
// the returned root.
//
// For a resolution that spans several files: `use <file>` builds a separate
// root Scope per used file, but all of them are read back through one
// evaluation, so their nodes' scopes have to land in ONE table. A per-root
// table would leave the outer evaluation unable to see anything the nested
// resolutions recorded -- every node of a used file would read back as
// having no scope.
std::unique_ptr<Scope> buildScopesInto(const std::vector<std::unique_ptr<ASTNode>>& ast, ScopeTable& table);
std::unique_ptr<Scope> buildScopesInto(const std::vector<ASTNode*>& ast, ScopeTable& table);

// Makes a trailing comma in a CALL ARGUMENT LIST a syntax error for as long
// as it is in scope, matching OpenSCAD 2021.01:
//
//     cube(1,);                  // error while this is in scope
//     echo(let(x = 1, y = 2,) x) // error -- let() parses as a call
//     a = [2, 4,];               // still fine: 2021.01 accepts it too
//     module m(a, b,) {}         // still fine, same reason
//
// One production (`arguments`) feeds every call form -- module
// instantiation, function calls, echo, assert, render and let -- so this is
// one rule, not a family of them. Parameter lists and list literals are
// deliberately untouched: 2021.01 accepts a trailing comma in both, and
// rejecting them would fail files it loads happily.
//
// A scope object rather than a parameter on all nine parse entry points:
// this is a parse-wide mode, the same shape ParseNumberingScope already
// uses, and threading a bool through getASTFromString/getASTFromFile/
// getASTFromLibraryFile/getProgramFromFile and their helpers would touch
// every one of them to say the same thing. Nests and restores on scope
// exit, exceptions included; thread-local, so one thread parsing strictly
// cannot change what another thread sees.
//
// getProgramFromFile()'s cache keys on this too -- a strict parse must not
// be handed a tree an earlier lenient parse of the same file left behind.
class StrictCommaScope {
public:
    StrictCommaScope();
    ~StrictCommaScope();
    StrictCommaScope(const StrictCommaScope&) = delete;
    StrictCommaScope& operator=(const StrictCommaScope&) = delete;

private:
    bool previous_;
};

// Whether a StrictCommaScope is currently in effect on this thread.
bool strictCommasEnabled();

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
// This does NOT share or cache anything: every call re-parses the file and
// every file it includes, and the caller owns all of it. See
// getProgramFromFile() for the shared, cached form -- the one an evaluator
// that re-renders the same script should use.
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

// A file's statements with its `include <...>` directives resolved, where
// each included file's AST is SHARED with every other file that includes
// it rather than re-parsed.
//
// That sharing is the point. Parsing `include <BOSL2/std.scad>` costs
// ~55ms and is ~82% of evaluating a small BOSL2 script; the library does
// not change between renders, so re-parsing it every time is the single
// largest cost in a re-render or a docs build (which renders ~1000
// examples, each including the same library).
//
// Only the per-FILE parses are cached, not a whole resolved program: a
// file is spliced in at most once per resolution (`visited`), so caching
// std.scad already-resolved would double its statements in a script that
// also includes something else depending on it. Re-running the resolution
// is only pointer pushes.
struct ParsedProgram {
    // The flattened statement list, in source order, includes spliced in
    // where their directives stood. Non-owning: see keepAlive.
    std::vector<const ASTNode*> nodes;
    // Holds every AST the above points into -- this file's own parse and
    // each shared include -- alive for as long as this object. Nodes are
    // borrowed, never owned, which is what lets two scripts (or two
    // threads) use the same parsed library at once.
    std::vector<std::shared_ptr<const std::vector<std::unique_ptr<ASTNode>>>> keepAlive;
};

// Parses `file` and resolves its includes against the cache, which is keyed
// by (path, mtime, size) so an edited file re-parses on its own.
// Thread-safe. Throws exactly as getASTFromFile does.
ParsedProgram getProgramFromFile(const std::string& file, bool includeComments = false);

// Drops every cached file parse. Nothing already handed out is
// invalidated -- a ParsedProgram keeps what it borrowed alive.
void clearAstCache();

// How many file parses the cache is holding. For tests that need to prove
// the cache replaces an edited file's entry rather than accumulating one
// per save.
size_t astCacheSize();

} // namespace oscad
