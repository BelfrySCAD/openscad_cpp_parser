#include "openscad_cpp_parser/ast/module_instantiation.hpp"

#include "format_utils.hpp"
#include "openscad_cpp_parser/ast/scope_builder.hpp"
#include "openscad_cpp_parser/scope.hpp"

namespace oscad {

namespace {

// Matches nodes.py's __str__ for module-instantiation children: a bare
// call with no children (e.g. `_cube_call()`) prints as just "cube(1)",
// with nothing appended -- no trailing semicolon, matching Assignment's
// own __str__ likewise having no statement terminator (that's added by
// the STATEMENT-level printer, not the node's own __str__). Exact
// OpenSCAD-source round-trip formatting (brace placement, indentation,
// semicolons) is pretty_print.cpp's job, not toString()'s -- see the note
// in expression.cpp's NumberLiteral::toString().
std::string formatChildBlock(const std::vector<std::unique_ptr<ASTNode>>& children) {
    if (children.empty()) {
        return "";
    }
    if (children.size() == 1) {
        return " " + children.front()->toString();
    }
    std::string s = " { ";
    for (const auto& c : children) {
        s += c->toString() + " ";
    }
    s += "}";
    return s;
}

} // namespace

std::string ModularCall::toString() const {
    // Deliberate deviation from nodes.py: the reference's ModularCall.__str__
    // is `f"{self.name}({args})"` and never renders `children`, even when
    // non-empty -- e.g. `translate([1,0,0]) cube(1);` would str() as just
    // "translate([1, 0, 0])", silently dropping the child. Every other
    // ModuleInstantiation subclass (ModularFor, ModularIf, ...) DOES render
    // its body/children; build_scope itself processes ModularCall.children
    // too, so the omission is asymmetric with the rest of the class and with
    // its own scope-building logic. The reference test suite never exercises
    // str() on a ModularCall with non-empty children, so this bug is
    // unexercised rather than intentional. Same category as the documented
    // BlankLine/CommentedExpr serialization-registry gap: fixed here rather
    // than replicated.
    return name->name + "(" + joinToString(arguments, ", ") + ")" + formatChildBlock(children);
}

void ModularCall::buildScope(Scope& parentScope) {
    setScope(parentScope);
    name->buildScope(parentScope);
    for (auto& a : arguments) {
        a->buildScope(parentScope);
    }
    if (!children.empty()) {
        Scope& childrenScope = parentScope.childScope();
        collectHoistedDeclarations(children, childrenScope);
        for (auto& c : children) {
            c->buildScope(childrenScope);
        }
    }
}

std::string ModularFor::toString() const {
    return "for (" + joinToString(assignments, ", ") + ")" + formatChildBlock(body);
}

void ModularFor::buildScope(Scope& parentScope) {
    setScope(parentScope);
    Scope& forScope = parentScope.childScope();
    for (auto& a : assignments) {
        forScope.defineVariable(a->name->name, a.get());
        a->buildScopeSplit(forScope, parentScope);
    }
    collectHoistedDeclarations(body, forScope);
    for (auto& n : body) {
        n->buildScope(forScope);
    }
}

std::string ModularIntersectionFor::toString() const {
    return "intersection_for (" + joinToString(assignments, ", ") + ")" + formatChildBlock(body);
}

void ModularIntersectionFor::buildScope(Scope& parentScope) {
    setScope(parentScope);
    Scope& forScope = parentScope.childScope();
    for (auto& a : assignments) {
        forScope.defineVariable(a->name->name, a.get());
        a->buildScopeSplit(forScope, parentScope);
    }
    collectHoistedDeclarations(body, forScope);
    for (auto& n : body) {
        n->buildScope(forScope);
    }
}

std::string ModularLet::toString() const {
    // Note the space before "(": distinguishes the statement/modular form
    // ("let (x = 1) ...") from the expression form LetOp ("let(x = 1) ..."
    // -- no space), matching the reference exactly.
    return "let (" + joinToString(assignments, ", ") + ")" + formatChildBlock(children);
}

void ModularLet::buildScope(Scope& parentScope) {
    setScope(parentScope);
    Scope& letScope = parentScope.childScope();
    for (auto& a : assignments) {
        letScope.defineVariable(a->name->name, a.get());
        a->buildScope(letScope);
    }
    collectHoistedDeclarations(children, letScope);
    for (auto& c : children) {
        c->buildScope(letScope);
    }
}

std::string ModularEcho::toString() const {
    return "echo(" + joinToString(arguments, ", ") + ")" + formatChildBlock(children);
}

void ModularEcho::buildScope(Scope& parentScope) {
    setScope(parentScope);
    for (auto& a : arguments) {
        a->buildScope(parentScope);
    }
    if (!children.empty()) {
        Scope& childrenScope = parentScope.childScope();
        collectHoistedDeclarations(children, childrenScope);
        for (auto& c : children) {
            c->buildScope(childrenScope);
        }
    }
}

std::string ModularAssert::toString() const {
    return "assert(" + joinToString(arguments, ", ") + ")" + formatChildBlock(children);
}

void ModularAssert::buildScope(Scope& parentScope) {
    setScope(parentScope);
    for (auto& a : arguments) {
        a->buildScope(parentScope);
    }
    if (!children.empty()) {
        Scope& childrenScope = parentScope.childScope();
        collectHoistedDeclarations(children, childrenScope);
        for (auto& c : children) {
            c->buildScope(childrenScope);
        }
    }
}

std::string ModularIf::toString() const {
    return "if (" + condition->toString() + ")" + formatChildBlock(trueBranch);
}

void ModularIf::buildScope(Scope& parentScope) {
    setScope(parentScope);
    condition->buildScope(parentScope);
    Scope& trueScope = parentScope.childScope();
    collectHoistedDeclarations(trueBranch, trueScope);
    for (auto& n : trueBranch) {
        n->buildScope(trueScope);
    }
}

std::string ModularIfElse::toString() const {
    return "if (" + condition->toString() + ")" + formatChildBlock(trueBranch) + " else" + formatChildBlock(falseBranch);
}

void ModularIfElse::buildScope(Scope& parentScope) {
    setScope(parentScope);
    condition->buildScope(parentScope);
    // Independent sibling scopes -- neither branch's scope is nested in the
    // other's.
    Scope& trueScope = parentScope.childScope();
    collectHoistedDeclarations(trueBranch, trueScope);
    for (auto& n : trueBranch) {
        n->buildScope(trueScope);
    }
    Scope& falseScope = parentScope.childScope();
    collectHoistedDeclarations(falseBranch, falseScope);
    for (auto& n : falseBranch) {
        n->buildScope(falseScope);
    }
}

#define OSCAD_MODIFIER_BUILD_SCOPE(ClassName)                                                                         \
    void ClassName::buildScope(Scope& parentScope) {                                                                \
        setScope(parentScope);                                                                                       \
        child->buildScope(parentScope);                                                                              \
    }

OSCAD_MODIFIER_BUILD_SCOPE(ModularModifierShowOnly)
OSCAD_MODIFIER_BUILD_SCOPE(ModularModifierHighlight)
OSCAD_MODIFIER_BUILD_SCOPE(ModularModifierBackground)
OSCAD_MODIFIER_BUILD_SCOPE(ModularModifierDisable)

#undef OSCAD_MODIFIER_BUILD_SCOPE

} // namespace oscad
