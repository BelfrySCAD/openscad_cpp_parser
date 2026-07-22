#pragma once

#include "openscad_cpp_parser/ast.hpp"

#include <memory>
#include <string>
#include <vector>

namespace oscad {

// Formats a parsed AST back into OpenSCAD source text. Round-trips
// parse(toOpenscad(parse(src))) to a structurally equivalent AST, though
// not necessarily byte-identical text. Mirrors pretty_print.py::to_openscad.
std::string toOpenscad(const std::vector<std::unique_ptr<ASTNode>>& nodes, int indentWidth = 4);

} // namespace oscad
