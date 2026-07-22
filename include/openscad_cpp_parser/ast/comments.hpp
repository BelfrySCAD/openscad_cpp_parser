#pragma once

#include "openscad_cpp_parser/ast/ast_node.hpp"

namespace oscad {

// A single-line `//text` comment. Only produced when comment preservation
// is requested (getASTFromString/File(includeComments=true)).
class CommentLine : public ASTNode {
public:
    CommentLine(Position position, std::string text)
        : ASTNode(NodeKind::CommentLine, std::move(position)), text(std::move(text)) {}

    std::string text;

    std::string toString() const override { return "//" + text; }
};

// A preserved blank line between consecutive single-line comment blocks.
class BlankLine : public ASTNode {
public:
    explicit BlankLine(Position position) : ASTNode(NodeKind::BlankLine, std::move(position)) {}

    std::string toString() const override { return ""; }
};

// A `/*text*/` multi-line comment span.
class CommentSpan : public ASTNode {
public:
    CommentSpan(Position position, std::string text)
        : ASTNode(NodeKind::CommentSpan, std::move(position)), text(std::move(text)) {}

    std::string text;

    std::string toString() const override { return "/*" + text + "*/"; }
};

} // namespace oscad
