#include "openscad_cpp_parser/api.hpp"

#include "openscad_cpp_parser/ast/scope_builder.hpp"

namespace oscad {

std::unique_ptr<Scope> buildScopes(const std::vector<std::unique_ptr<ASTNode>>& ast) {
    auto root = std::make_unique<Scope>();
    collectHoistedDeclarations(ast, *root);
    for (auto& node : ast) {
        node->buildScope(*root);
    }
    return root;
}

} // namespace oscad
