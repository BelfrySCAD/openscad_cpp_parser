#pragma once

#include "openscad_cpp_parser/ast/ast_node.hpp"

#include <memory>
#include <vector>

namespace oscad {

class Scope;

// Scans a sibling list and pre-registers Assignment/FunctionDeclaration/
// ModuleDeclaration nodes into `scope`, before per-node buildScope()
// recursion runs on that list. Implements OpenSCAD's forward-reference
// (hoisting) semantics within any block (module body, for/if/let body, top
// level). Mirrors nodes.py's module-level `_collect_hoisted_declarations`.
void collectHoistedDeclarations(const std::vector<std::unique_ptr<ASTNode>>& nodes, Scope& scope);

// Same, but for nodes referenced by raw pointer rather than owned via
// unique_ptr -- for building a scope over nodes that live in more than one
// independently-owned AST vector at once (e.g. a `use <file>` statement's
// evaluator-level resolution, which combines declarations pulled in from
// another file's already-parsed-and-owned AST with the current file's own
// nodes). Never takes ownership.
void collectHoistedDeclarations(const std::vector<ASTNode*>& nodes, Scope& scope);

} // namespace oscad
