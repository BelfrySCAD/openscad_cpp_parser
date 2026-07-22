#pragma once

#include "openscad_cpp_parser/ast/expression.hpp"

#include <memory>
#include <vector>

namespace oscad {

// Base for list-comprehension clauses (`for`/`if`/`let`/`each`/c-style
// `for`). VectorElement is a SIBLING of Expression, not a subclass -- a
// clause like `for(...)` doesn't itself produce a value.
//
// Per the grammar, `?vector_element: listcomp_elements | expr` -- any field
// typed "VectorElement" below can actually hold a plain Expression at
// runtime (e.g. the innermost `i` in `[for (i=[1,2]) i]`), so those fields
// are typed as the common base ASTNode, not VectorElement.
class VectorElement : public ASTNode {
protected:
    using ASTNode::ASTNode;
};

class ListCompLet : public VectorElement {
public:
    ListCompLet(Position position, std::vector<std::unique_ptr<Assignment>> assignments, std::unique_ptr<ASTNode> body)
        : VectorElement(NodeKind::ListCompLet, std::move(position)), assignments(std::move(assignments)),
          body(std::move(body)) {}

    std::vector<std::unique_ptr<Assignment>> assignments;
    std::unique_ptr<ASTNode> body;

    std::string toString() const override;
    void buildScope(Scope& parentScope) override;
};

class ListCompEach : public VectorElement {
public:
    ListCompEach(Position position, std::unique_ptr<ASTNode> body)
        : VectorElement(NodeKind::ListCompEach, std::move(position)), body(std::move(body)) {}

    std::unique_ptr<ASTNode> body;

    std::string toString() const override { return "each " + body->toString(); }
    void buildScope(Scope& parentScope) override;
};

// `for (assignments...) body` -- loop-variable RHS is built against the
// OUTER scope (not the new for_scope), asymmetric with ListCompCFor. See
// buildScope() in vector_element.cpp.
class ListCompFor : public VectorElement {
public:
    ListCompFor(Position position, std::vector<std::unique_ptr<Assignment>> assignments, std::unique_ptr<ASTNode> body)
        : VectorElement(NodeKind::ListCompFor, std::move(position)), assignments(std::move(assignments)),
          body(std::move(body)) {}

    std::vector<std::unique_ptr<Assignment>> assignments;
    std::unique_ptr<ASTNode> body;

    std::string toString() const override;
    void buildScope(Scope& parentScope) override;
};

// C-style `for (inits; condition; incrs) body` -- unlike ListCompFor, each
// init's RHS IS built in the new for_scope (so inits can chain: `i=0, j=i+1`).
class ListCompCFor : public VectorElement {
public:
    ListCompCFor(Position position, std::vector<std::unique_ptr<Assignment>> inits, std::unique_ptr<Expression> condition,
                 std::vector<std::unique_ptr<Assignment>> incrs, std::unique_ptr<ASTNode> body)
        : VectorElement(NodeKind::ListCompCFor, std::move(position)), inits(std::move(inits)),
          condition(std::move(condition)), incrs(std::move(incrs)), body(std::move(body)) {}

    std::vector<std::unique_ptr<Assignment>> inits;
    std::unique_ptr<Expression> condition;
    std::vector<std::unique_ptr<Assignment>> incrs;
    std::unique_ptr<ASTNode> body;

    std::string toString() const override;
    void buildScope(Scope& parentScope) override;
};

class ListCompIf : public VectorElement {
public:
    ListCompIf(Position position, std::unique_ptr<Expression> condition, std::unique_ptr<ASTNode> trueExpr)
        : VectorElement(NodeKind::ListCompIf, std::move(position)), condition(std::move(condition)),
          trueExpr(std::move(trueExpr)) {}

    std::unique_ptr<Expression> condition;
    std::unique_ptr<ASTNode> trueExpr;

    std::string toString() const override;
    void buildScope(Scope& parentScope) override;
};

class ListCompIfElse : public VectorElement {
public:
    ListCompIfElse(Position position, std::unique_ptr<Expression> condition, std::unique_ptr<ASTNode> trueExpr,
                    std::unique_ptr<ASTNode> falseExpr)
        : VectorElement(NodeKind::ListCompIfElse, std::move(position)), condition(std::move(condition)),
          trueExpr(std::move(trueExpr)), falseExpr(std::move(falseExpr)) {}

    std::unique_ptr<Expression> condition;
    std::unique_ptr<ASTNode> trueExpr;
    std::unique_ptr<ASTNode> falseExpr;

    std::string toString() const override;
    void buildScope(Scope& parentScope) override;
};

// Also how plain vector literals (`[1, 2, 3]`) are represented: elements
// are a heterogeneous mix of VectorElement clauses and raw Expressions
// (never wrapped), matching the grammar's `?vector_element: listcomp_elements
// | expr`.
class ListComprehension : public Expression {
public:
    ListComprehension(Position position, std::vector<std::unique_ptr<ASTNode>> elements)
        : Expression(NodeKind::ListComprehension, std::move(position)), elements(std::move(elements)) {}

    std::vector<std::unique_ptr<ASTNode>> elements;

    std::string toString() const override;
    void buildScope(Scope& parentScope) override;
};

} // namespace oscad
