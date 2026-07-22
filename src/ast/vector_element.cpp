#include "openscad_cpp_parser/ast/vector_element.hpp"

#include "format_utils.hpp"
#include "openscad_cpp_parser/scope.hpp"

namespace oscad {

std::string ListCompLet::toString() const {
    return "let(" + joinToString(assignments, ", ") + ") " + body->toString();
}

void ListCompLet::buildScope(Scope& parentScope) {
    setScope(parentScope);
    Scope& letScope = parentScope.childScope();
    for (auto& a : assignments) {
        letScope.defineVariable(a->name->name, a.get());
        a->buildScope(letScope);
    }
    body->buildScope(letScope);
}

void ListCompEach::buildScope(Scope& parentScope) {
    setScope(parentScope);
    body->buildScope(parentScope);
}

std::string ListCompFor::toString() const {
    return "for (" + joinToString(assignments, ", ") + ") " + body->toString();
}

void ListCompFor::buildScope(Scope& parentScope) {
    setScope(parentScope);
    Scope& forScope = parentScope.childScope();
    // Loop-variable RHS resolves against the OUTER scope, not forScope --
    // `[for (i = i) i]` sees the outer `i` in the assignment's RHS.
    for (auto& a : assignments) {
        forScope.defineVariable(a->name->name, a.get());
        a->buildScopeSplit(forScope, parentScope);
    }
    body->buildScope(forScope);
}

std::string ListCompCFor::toString() const {
    return "for (" + joinToString(inits, ", ") + "; " + condition->toString() + "; " + joinToString(incrs, ", ") + ") " +
           body->toString();
}

void ListCompCFor::buildScope(Scope& parentScope) {
    setScope(parentScope);
    Scope& forScope = parentScope.childScope();
    // Unlike ListCompFor: inits' RHS IS built in forScope, so inits can
    // chain (`i=0, j=i+1`).
    for (auto& init : inits) {
        forScope.defineVariable(init->name->name, init.get());
        init->buildScope(forScope);
    }
    condition->buildScope(forScope);
    for (auto& incr : incrs) {
        incr->buildScope(forScope);
    }
    body->buildScope(forScope);
}

std::string ListCompIf::toString() const {
    return "if (" + condition->toString() + ") " + trueExpr->toString();
}

void ListCompIf::buildScope(Scope& parentScope) {
    setScope(parentScope);
    condition->buildScope(parentScope);
    trueExpr->buildScope(parentScope);
}

std::string ListCompIfElse::toString() const {
    return "if (" + condition->toString() + ") " + trueExpr->toString() + " else " + falseExpr->toString();
}

void ListCompIfElse::buildScope(Scope& parentScope) {
    setScope(parentScope);
    condition->buildScope(parentScope);
    trueExpr->buildScope(parentScope);
    falseExpr->buildScope(parentScope);
}

std::string ListComprehension::toString() const {
    return "[" + joinToString(elements, ", ") + "]";
}

void ListComprehension::buildScope(Scope& parentScope) {
    setScope(parentScope);
    for (auto& e : elements) {
        e->buildScope(parentScope);
    }
}

} // namespace oscad
