#pragma once

#include "openscad_cpp_parser/ast.hpp"

#include <memory>
#include <string>
#include <vector>

namespace oscad {

// Re-scans the original source text for comments (already stripped by the
// lexer for the main parse). Standalone comments (on their own line) and
// preserved blank lines are spliced into the top-level node list; inline
// comments (sharing a line with code, e.g. `x = 1; // note`) are wrapped
// around the nearest expression as a CommentedExpr, falling back to the
// last expression of the nearest preceding node if no exact match is
// found. Mirrors __init__.py's _extract_comments + _classify_comments +
// _attach_inline_comments + _inject_comments (see
// src/inline_comment_attach.cpp and its classifyNode() for where this
// deliberately simplifies the reference's generic reflection walk --
// declarative-identity fields like names are never wrap targets, and
// nested module-instantiation children are visited more completely than
// the reference's one-level-only container unwrap actually manages to).
std::vector<std::unique_ptr<ASTNode>> attachComments(std::vector<std::unique_ptr<ASTNode>> astNodes, const std::string& code,
                                                       const std::string& origin);

} // namespace oscad
