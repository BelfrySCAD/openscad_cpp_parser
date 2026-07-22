#pragma once

#include "openscad_cpp_parser/api.hpp"

// Shared helpers for the ported test suite, mirroring the Python reference
// suite's `parse` pytest fixture and `_expr`/`_getast` local helpers.
namespace oscad {

// Mirrors conftest.py's `parse` fixture (a thin getASTfromString wrapper).
inline std::vector<std::unique_ptr<ASTNode>> parseSrc(const std::string& code) {
    return getASTFromString(code);
}

// Mirrors the reference suite's `_expr(code)` helper: wraps `code` as
// `x = {code};`, parses it, and returns the assignment's RHS expression.
// The returned pointer is only valid as long as the caller keeps `ast`
// (the out-param) alive.
inline Expression* exprSrc(const std::string& code, std::vector<std::unique_ptr<ASTNode>>& ast) {
    ast = getASTFromString("x = " + code + ";");
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    return a ? a->expr.get() : nullptr;
}

} // namespace oscad
