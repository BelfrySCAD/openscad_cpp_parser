#pragma once

#include "openscad_cpp_parser/ast.hpp"

#include <memory>
#include <string>
#include <vector>

namespace oscad {

// Wraps expressions adjacent to `inlineComments` in CommentedExpr, mutating
// `astNodes`' subtree in place. Comments that fall within some node's span
// but don't line up with a specific expression there are attached to the
// nearest preceding top-level node's last expression field as a trailing
// comment (mirrors _attach_inline_comments's fallback pass). Comments that
// still can't be matched to anything are dropped.
void attachInlineComments(std::vector<std::unique_ptr<ASTNode>>& astNodes,
                           std::vector<std::unique_ptr<ASTNode>> inlineComments);

// Claims `/* */` block comments (CommentSpan only -- these fields' own
// declared type) that sit in the structural gaps a FunctionDeclaration or
// ModuleDeclaration doesn't expose as any Expression slot: before the name,
// between the name and the parameter list, between adjacent parameters
// (including a parameter with no default value, which classifyNode's own
// exprField walk never visits at all), and between the parameter list and
// the body. Mutates `astNodes` (populating each declaration's own
// preNameComments/postNameComments/postParamsComments and each
// ParameterDeclaration's leadingComments/trailingComments) and nulls out
// whatever it claims from `comments`, matching attachInlineComments' own
// "claim by nulling the shared vector slot" convention. Must run BEFORE
// attachInlineComments/the inline-vs-standalone split -- a `/* */` comment
// alone on its own line before a parameter is classified *standalone* by
// isInlineComment, which this pass still needs to claim, so it operates on
// the full extracted comment list, not just the inline subset.
void attachDeclarationComments(std::vector<std::unique_ptr<ASTNode>>& astNodes, const std::string& code,
                                std::vector<std::unique_ptr<ASTNode>>& comments);

} // namespace oscad
