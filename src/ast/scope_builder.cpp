#include "openscad_cpp_parser/ast/scope_builder.hpp"

#include "openscad_cpp_parser/ast/declarations.hpp"
#include "openscad_cpp_parser/ast/expression.hpp"
#include "openscad_cpp_parser/scope.hpp"

namespace oscad {

void collectHoistedDeclarations(const std::vector<std::unique_ptr<ASTNode>>& nodes, Scope& scope) {
    for (const auto& n : nodes) {
        if (auto* a = dynamic_cast<Assignment*>(n.get())) {
            scope.defineVariable(a->name->name, a);
        } else if (auto* f = dynamic_cast<FunctionDeclaration*>(n.get())) {
            scope.defineFunction(f->name->name, f);
        } else if (auto* m = dynamic_cast<ModuleDeclaration*>(n.get())) {
            scope.defineModule(m->name->name, m);
        }
    }
}

} // namespace oscad
