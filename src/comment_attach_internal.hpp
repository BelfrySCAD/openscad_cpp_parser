#pragma once

#include "openscad_cpp_parser/ast.hpp"

#include <memory>
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

} // namespace oscad
