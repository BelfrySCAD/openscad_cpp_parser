#include "openscad_cpp_parser/ast/declarations.hpp"

#include "format_utils.hpp"
#include "openscad_cpp_parser/ast/scope_builder.hpp"
#include "openscad_cpp_parser/scope.hpp"

namespace oscad {

namespace {
// Matches nodes.py's ModuleDeclaration.__str__: single-line, space-joined
// children (e.g. "module m() { cube(1) }"), not the multi-line indented
// form pretty_print.cpp's toOpenscad() produces -- this is a debug repr.
std::string formatBlock(const std::vector<std::unique_ptr<ASTNode>>& children) {
    if (children.empty()) {
        return " {}";
    }
    std::string s = " { ";
    for (size_t i = 0; i < children.size(); ++i) {
        if (i != 0) {
            s += " ";
        }
        s += children[i]->toString();
    }
    s += " }";
    return s;
}
} // namespace

std::string ModuleDeclaration::toString() const {
    return "module " + name->name + "(" + joinToString(parameters, ", ") + ")" + formatBlock(children);
}

void ModuleDeclaration::buildScope(Scope& parentScope) {
    setScope(parentScope);
    for (auto& c : preNameComments) {
        c->buildScope(parentScope);
    }
    name->buildScope(parentScope);
    for (auto& c : postNameComments) {
        c->buildScope(parentScope);
    }
    Scope& modScope = parentScope.childScope();
    for (auto& p : parameters) {
        modScope.defineVariable(p->name->name, p.get());
        p->buildScope(modScope);
    }
    for (auto& c : postParamsComments) {
        c->buildScope(modScope);
    }
    collectHoistedDeclarations(children, modScope);
    for (auto& c : children) {
        c->buildScope(modScope);
    }
}

std::string FunctionDeclaration::toString() const {
    return "function " + name->name + "(" + joinToString(parameters, ", ") + ") = " + expr->toString() + ";";
}

void FunctionDeclaration::buildScope(Scope& parentScope) {
    setScope(parentScope);
    for (auto& c : preNameComments) {
        c->buildScope(parentScope);
    }
    name->buildScope(parentScope);
    for (auto& c : postNameComments) {
        c->buildScope(parentScope);
    }
    Scope& funcScope = parentScope.childScope();
    for (auto& p : parameters) {
        funcScope.defineVariable(p->name->name, p.get());
        p->buildScope(funcScope);
    }
    for (auto& c : postParamsComments) {
        c->buildScope(funcScope);
    }
    expr->buildScope(funcScope);
}

} // namespace oscad
