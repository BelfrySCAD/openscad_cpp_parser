#pragma once

#include "openscad_cpp_parser/ast/expression.hpp"

#include <memory>
#include <vector>

namespace oscad {

// Base for the "geometry"/statement side of OpenSCAD (produces geometry,
// not a value). A SIBLING of Expression, rooted at ASTNode.
class ModuleInstantiation : public ASTNode {
protected:
    using ASTNode::ASTNode;
};

// NOTE: children/body/branch lists below are typed `vector<unique_ptr<
// ASTNode>>`, not `vector<unique_ptr<ModuleInstantiation>>`. A `{ ... }`
// block is `statement*`, which allows Assignment/FunctionDeclaration/
// ModuleDeclaration interspersed with module instantiations (e.g.
// `module foo() { x = 1; cube(x); }`) -- this is exactly why buildScope()
// on these nodes calls collectHoistedDeclarations() on these lists before
// recursing. Also always a list at parse time even for a single bare
// statement or empty body (confirmed against transformer.py::
// child_statement, which normalizes every case to a list), despite the
// Python dataclass annotations reading singular ModuleInstantiation.

class ModularCall : public ModuleInstantiation {
public:
    ModularCall(Position position, std::unique_ptr<Identifier> name, std::vector<std::unique_ptr<Argument>> arguments,
                std::vector<std::unique_ptr<ASTNode>> children)
        : ModuleInstantiation(NodeKind::ModularCall, std::move(position)), name(std::move(name)),
          arguments(std::move(arguments)), children(std::move(children)) {}

    std::unique_ptr<Identifier> name;
    std::vector<std::unique_ptr<Argument>> arguments;
    std::vector<std::unique_ptr<ASTNode>> children;

    std::string toString() const override;
    void buildScope(Scope& parentScope) override;
};

class ModularFor : public ModuleInstantiation {
public:
    ModularFor(Position position, std::vector<std::unique_ptr<Assignment>> assignments,
               std::vector<std::unique_ptr<ASTNode>> body)
        : ModuleInstantiation(NodeKind::ModularFor, std::move(position)), assignments(std::move(assignments)),
          body(std::move(body)) {}

    std::vector<std::unique_ptr<Assignment>> assignments;
    std::vector<std::unique_ptr<ASTNode>> body;

    std::string toString() const override;
    void buildScope(Scope& parentScope) override;
};

class ModularIntersectionFor : public ModuleInstantiation {
public:
    ModularIntersectionFor(Position position, std::vector<std::unique_ptr<Assignment>> assignments,
                            std::vector<std::unique_ptr<ASTNode>> body)
        : ModuleInstantiation(NodeKind::ModularIntersectionFor, std::move(position)), assignments(std::move(assignments)),
          body(std::move(body)) {}

    std::vector<std::unique_ptr<Assignment>> assignments;
    std::vector<std::unique_ptr<ASTNode>> body;

    std::string toString() const override;
    void buildScope(Scope& parentScope) override;
};

class ModularLet : public ModuleInstantiation {
public:
    ModularLet(Position position, std::vector<std::unique_ptr<Assignment>> assignments,
               std::vector<std::unique_ptr<ASTNode>> children)
        : ModuleInstantiation(NodeKind::ModularLet, std::move(position)), assignments(std::move(assignments)),
          children(std::move(children)) {}

    std::vector<std::unique_ptr<Assignment>> assignments;
    std::vector<std::unique_ptr<ASTNode>> children;

    std::string toString() const override;
    void buildScope(Scope& parentScope) override;
};

class ModularEcho : public ModuleInstantiation {
public:
    ModularEcho(Position position, std::vector<std::unique_ptr<Argument>> arguments,
                std::vector<std::unique_ptr<ASTNode>> children)
        : ModuleInstantiation(NodeKind::ModularEcho, std::move(position)), arguments(std::move(arguments)),
          children(std::move(children)) {}

    std::vector<std::unique_ptr<Argument>> arguments;
    std::vector<std::unique_ptr<ASTNode>> children;

    std::string toString() const override;
    void buildScope(Scope& parentScope) override;
};

class ModularAssert : public ModuleInstantiation {
public:
    ModularAssert(Position position, std::vector<std::unique_ptr<Argument>> arguments,
                  std::vector<std::unique_ptr<ASTNode>> children)
        : ModuleInstantiation(NodeKind::ModularAssert, std::move(position)), arguments(std::move(arguments)),
          children(std::move(children)) {}

    std::vector<std::unique_ptr<Argument>> arguments;
    std::vector<std::unique_ptr<ASTNode>> children;

    std::string toString() const override;
    void buildScope(Scope& parentScope) override;
};

class ModularIf : public ModuleInstantiation {
public:
    ModularIf(Position position, std::unique_ptr<Expression> condition, std::vector<std::unique_ptr<ASTNode>> trueBranch)
        : ModuleInstantiation(NodeKind::ModularIf, std::move(position)), condition(std::move(condition)),
          trueBranch(std::move(trueBranch)) {}

    std::unique_ptr<Expression> condition;
    std::vector<std::unique_ptr<ASTNode>> trueBranch;

    std::string toString() const override;
    void buildScope(Scope& parentScope) override;
};

// True and false branches get two INDEPENDENT sibling child scopes (neither
// nested in the other).
class ModularIfElse : public ModuleInstantiation {
public:
    ModularIfElse(Position position, std::unique_ptr<Expression> condition, std::vector<std::unique_ptr<ASTNode>> trueBranch,
                  std::vector<std::unique_ptr<ASTNode>> falseBranch)
        : ModuleInstantiation(NodeKind::ModularIfElse, std::move(position)), condition(std::move(condition)),
          trueBranch(std::move(trueBranch)), falseBranch(std::move(falseBranch)) {}

    std::unique_ptr<Expression> condition;
    std::vector<std::unique_ptr<ASTNode>> trueBranch;
    std::vector<std::unique_ptr<ASTNode>> falseBranch;

    std::string toString() const override;
    void buildScope(Scope& parentScope) override;
};

// -- Modifier wrappers (`! # % *`) -- stack/nest, e.g. `#!cube(1);` --------

#define OSCAD_MODIFIER(ClassName, Symbol)                                                                             \
    class ClassName : public ModuleInstantiation {                                                                   \
    public:                                                                                                           \
        ClassName(Position position, std::unique_ptr<ModuleInstantiation> child)                                     \
            : ModuleInstantiation(NodeKind::ClassName, std::move(position)), child(std::move(child)) {}               \
        std::unique_ptr<ModuleInstantiation> child;                                                                  \
        std::string toString() const override { return Symbol + child->toString(); }                                 \
        void buildScope(Scope& parentScope) override;                                                                \
    };

OSCAD_MODIFIER(ModularModifierShowOnly, std::string("!"))
OSCAD_MODIFIER(ModularModifierHighlight, std::string("#"))
OSCAD_MODIFIER(ModularModifierBackground, std::string("%"))
OSCAD_MODIFIER(ModularModifierDisable, std::string("*"))

#undef OSCAD_MODIFIER

} // namespace oscad
