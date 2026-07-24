#include "openscad_cpp_parser/ast/scope_builder.hpp"

#include "openscad_cpp_parser/ast/declarations.hpp"
#include "openscad_cpp_parser/ast/expression.hpp"
#include "openscad_cpp_parser/scope.hpp"

namespace oscad {

void collectHoistedDeclarations(const std::vector<ASTNode*>& nodes, Scope& scope) {
    for (ASTNode* n : nodes) {
        if (auto* a = dynamic_cast<Assignment*>(n)) {
            scope.defineVariable(a->name->name, a);
        } else if (auto* f = dynamic_cast<FunctionDeclaration*>(n)) {
            scope.defineFunction(f->name->name, f);
        } else if (auto* m = dynamic_cast<ModuleDeclaration*>(n)) {
            scope.defineModule(m->name->name, m);
        }
    }
}

void collectHoistedDeclarations(const std::vector<std::unique_ptr<ASTNode>>& nodes, Scope& scope) {
    std::vector<ASTNode*> raw;
    raw.reserve(nodes.size());
    for (const auto& n : nodes) raw.push_back(n.get());
    collectHoistedDeclarations(raw, scope);
}

} // namespace oscad
