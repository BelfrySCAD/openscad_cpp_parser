#pragma once

#include "openscad_cpp_parser/ast/ast_node.hpp"
#include "openscad_cpp_parser/ast/comments.hpp"

#include <memory>
#include <string>
#include <vector>

namespace oscad {

// Base class for all OpenSCAD expressions (constructs that evaluate to a
// value): literals, operators, calls, variable references.
class Expression : public ASTNode {
protected:
    using ASTNode::ASTNode;
};

// Base class for atomic (non-decomposable) expressions: literals and
// identifiers.
class Primary : public Expression {
protected:
    using Expression::Expression;
};

class Identifier : public Primary {
public:
    Identifier(Position position, std::string name)
        : Primary(NodeKind::Identifier, std::move(position)), name(std::move(name)) {}

    std::string name;

    std::string toString() const override { return name; }
};

// Stored WITHOUT surrounding quotes; escape sequences are kept raw/undecoded
// (a source `\n` stays as the two characters backslash-n), matching the
// reference lexer.
class StringLiteral : public Primary {
public:
    StringLiteral(Position position, std::string val)
        : Primary(NodeKind::StringLiteral, std::move(position)), val(std::move(val)) {}

    std::string val;

    std::string toString() const override { return "\"" + val + "\""; }
};

// All OpenSCAD numbers (including hex integer literals) are stored as
// double, matching the reference transformer.
class NumberLiteral : public Primary {
public:
    NumberLiteral(Position position, double val)
        : Primary(NodeKind::NumberLiteral, std::move(position)), val(val) {}

    double val;

    std::string toString() const override;
};

class BooleanLiteral : public Primary {
public:
    BooleanLiteral(Position position, bool val)
        : Primary(NodeKind::BooleanLiteral, std::move(position)), val(val) {}

    bool val;

    std::string toString() const override { return val ? "true" : "false"; }
};

class UndefinedLiteral : public Primary {
public:
    explicit UndefinedLiteral(Position position) : Primary(NodeKind::UndefinedLiteral, std::move(position)) {}

    std::string toString() const override { return "undef"; }
};

// `[start:step:end]` (or `[start:end]` in source, with step defaulted to
// NumberLiteral(1.0) by the parser when omitted).
class RangeLiteral : public Primary {
public:
    RangeLiteral(Position position, std::unique_ptr<Expression> start, std::unique_ptr<Expression> end,
                 std::unique_ptr<Expression> step, bool implicitStep = false)
        : Primary(NodeKind::RangeLiteral, std::move(position)), start(std::move(start)), end(std::move(end)),
          step(std::move(step)), implicitStep(implicitStep) {}

    std::unique_ptr<Expression> start;
    std::unique_ptr<Expression> end;
    std::unique_ptr<Expression> step;

    // True when the source wrote the two-argument form `[a:b]` and the step
    // node below is the literal 1.0 this parser synthesized for it. Consumers
    // that only need the value can ignore this and read `step` as always
    // present; it exists because "the author did not choose a step" is not
    // recoverable from the synthesized node, and the evaluator's backwards-range
    // warning fires only for that case (an explicit step is taken as deliberate).
    bool implicitStep = false;

    std::string toString() const override;
    void buildScope(Scope& parentScope) override;
};

// Base class for function/module call arguments.
class Argument : public ASTNode {
protected:
    using ASTNode::ASTNode;
};

class PositionalArgument : public Argument {
public:
    PositionalArgument(Position position, std::unique_ptr<Expression> expr)
        : Argument(NodeKind::PositionalArgument, std::move(position)), expr(std::move(expr)) {}

    std::unique_ptr<Expression> expr;

    std::string toString() const override { return expr->toString(); }
    void buildScope(Scope& parentScope) override;
};

class NamedArgument : public Argument {
public:
    NamedArgument(Position position, std::unique_ptr<Identifier> name, std::unique_ptr<Expression> expr)
        : Argument(NodeKind::NamedArgument, std::move(position)), name(std::move(name)), expr(std::move(expr)) {}

    std::unique_ptr<Identifier> name;
    std::unique_ptr<Expression> expr;

    std::string toString() const override { return name->name + " = " + expr->toString(); }
    void buildScope(Scope& parentScope) override;
};

// A parameter in a function/module definition. Note the caller-scope rule
// for `default`: it is built against the *definition site's enclosing*
// scope (parentScope.parent()), not the function/module's own new scope --
// see buildScope() in expression.cpp.
class ParameterDeclaration : public ASTNode {
public:
    ParameterDeclaration(Position position, std::unique_ptr<Identifier> name, std::unique_ptr<Expression> defaultValue)
        : ASTNode(NodeKind::ParameterDeclaration, std::move(position)), name(std::move(name)),
          defaultValue(std::move(defaultValue)) {}

    std::unique_ptr<Identifier> name;
    std::unique_ptr<Expression> defaultValue; // nullable
    std::vector<std::unique_ptr<CommentSpan>> leadingComments;
    std::vector<std::unique_ptr<CommentSpan>> trailingComments;

    std::string toString() const override;
    void buildScope(Scope& parentScope) override;
};

// Used both as a top-level statement and inside let/for assignment lists.
class Assignment : public ASTNode {
public:
    Assignment(Position position, std::unique_ptr<Identifier> name, std::unique_ptr<Expression> expr)
        : ASTNode(NodeKind::Assignment, std::move(position)), name(std::move(name)), expr(std::move(expr)) {}

    std::unique_ptr<Identifier> name;
    std::unique_ptr<Expression> expr;

    std::string toString() const override { return name->name + " = " + expr->toString(); }
    void buildScope(Scope& parentScope) override;

    // Used by ListCompFor/ModularFor/ModularIntersectionFor: the loop
    // variable's name is bound in the new per-iteration scope, but its RHS
    // must still resolve against the outer scope (so `for (i = i)` sees the
    // outer `i`, not itself) -- an asymmetry ordinary buildScope() can't
    // express since it uses a single scope for both.
    void buildScopeSplit(Scope& nameScope, Scope& exprScope);
};

// An expression preceded/followed by inline block comments (only produced
// when comment preservation is enabled). Comment entries are CommentLine or
// CommentSpan.
class CommentedExpr : public Expression {
public:
    CommentedExpr(Position position, std::vector<std::unique_ptr<ASTNode>> leadingComments,
                  std::vector<std::unique_ptr<ASTNode>> trailingComments, std::unique_ptr<Expression> expr)
        : Expression(NodeKind::CommentedExpr, std::move(position)), leadingComments(std::move(leadingComments)),
          trailingComments(std::move(trailingComments)), expr(std::move(expr)) {}

    std::vector<std::unique_ptr<ASTNode>> leadingComments;
    std::vector<std::unique_ptr<ASTNode>> trailingComments;
    std::unique_ptr<Expression> expr;

    std::string toString() const override;
    void buildScope(Scope& parentScope) override;
};

// `let(assignments...) body` -- bindings are sequential (later assignments'
// RHS can see earlier ones), and each assignment's name is defined in the
// new scope *before* its own RHS is built (so an RHS lookup of its own name
// resolves to itself, not an outer binding). See buildScope() in
// expression.cpp -- replicate this literally.
class LetOp : public Expression {
public:
    LetOp(Position position, std::vector<std::unique_ptr<Assignment>> assignments, std::unique_ptr<Expression> body)
        : Expression(NodeKind::LetOp, std::move(position)), assignments(std::move(assignments)), body(std::move(body)) {}

    std::vector<std::unique_ptr<Assignment>> assignments;
    std::unique_ptr<Expression> body;

    std::string toString() const override;
    void buildScope(Scope& parentScope) override;
};

// `echo(arguments...) body` -- body defaults to an UndefinedLiteral when
// omitted in source, so this field is never null.
class EchoOp : public Expression {
public:
    EchoOp(Position position, std::vector<std::unique_ptr<Argument>> arguments, std::unique_ptr<Expression> body)
        : Expression(NodeKind::EchoOp, std::move(position)), arguments(std::move(arguments)), body(std::move(body)) {}

    std::vector<std::unique_ptr<Argument>> arguments;
    std::unique_ptr<Expression> body;

    std::string toString() const override;
    void buildScope(Scope& parentScope) override;
};

// `assert(arguments...) body` -- same shape as EchoOp.
class AssertOp : public Expression {
public:
    AssertOp(Position position, std::vector<std::unique_ptr<Argument>> arguments, std::unique_ptr<Expression> body)
        : Expression(NodeKind::AssertOp, std::move(position)), arguments(std::move(arguments)), body(std::move(body)) {}

    std::vector<std::unique_ptr<Argument>> arguments;
    std::unique_ptr<Expression> body;

    std::string toString() const override;
    void buildScope(Scope& parentScope) override;
};

// Anonymous `function(parameters...) body` literal.
class FunctionLiteral : public Expression {
public:
    FunctionLiteral(Position position, std::vector<std::unique_ptr<ParameterDeclaration>> parameters,
                     std::unique_ptr<Expression> body)
        : Expression(NodeKind::FunctionLiteral, std::move(position)), parameters(std::move(parameters)),
          body(std::move(body)) {}

    std::vector<std::unique_ptr<ParameterDeclaration>> parameters;
    std::unique_ptr<Expression> body;

    std::string toString() const override;
    void buildScope(Scope& parentScope) override;
};

// `render()` in EXPRESSION position: `obj = render() { cube(10); };`
//
// Evaluates its children as geometry, measures the result, and yields an
// object() -- it draws nothing. The STATEMENT form (`render() cube(1);`)
// stays a plain ModularCall named "render"; this class exists only because
// an Expression cannot be a ModuleInstantiation (they are siblings under
// ASTNode, not parent/child).
//
// `arguments` and `children` deliberately mirror ModularCall's field types
// so the evaluator can hand them straight to resolveCallArgs()/evalChildren()
// with no adaptation. `children` is ASTNode, not ModuleInstantiation, for
// the same reason ModularCall's is -- a `{ ... }` block is `statement*` and
// may hold Assignments, which is why buildScope() below hoists.
class RenderExpression : public Expression {
public:
    RenderExpression(Position position, std::vector<std::unique_ptr<Argument>> arguments,
                     std::vector<std::unique_ptr<ASTNode>> children)
        : Expression(NodeKind::RenderExpression, std::move(position)), arguments(std::move(arguments)),
          children(std::move(children)) {}

    std::vector<std::unique_ptr<Argument>> arguments;
    std::vector<std::unique_ptr<ASTNode>> children;

    std::string toString() const override;
    void buildScope(Scope& parentScope) override;
};

// -- Unary operators --------------------------------------------------

#define OSCAD_UNARY_OP(ClassName)                                                                                    \
    class ClassName : public Expression {                                                                            \
    public:                                                                                                          \
        ClassName(Position position, std::unique_ptr<Expression> expr)                                               \
            : Expression(NodeKind::ClassName, std::move(position)), expr(std::move(expr)) {}                         \
        std::unique_ptr<Expression> expr;                                                                            \
        std::string toString() const override;                                                                       \
        void buildScope(Scope& parentScope) override;                                                                \
    };

OSCAD_UNARY_OP(UnaryMinusOp)
OSCAD_UNARY_OP(LogicalNotOp)
OSCAD_UNARY_OP(BitwiseNotOp)

#undef OSCAD_UNARY_OP

// -- Binary operators ---------------------------------------------------

#define OSCAD_BINARY_OP(ClassName)                                                                                    \
    class ClassName : public Expression {                                                                            \
    public:                                                                                                          \
        ClassName(Position position, std::unique_ptr<Expression> left, std::unique_ptr<Expression> right)            \
            : Expression(NodeKind::ClassName, std::move(position)), left(std::move(left)), right(std::move(right)) {} \
        std::unique_ptr<Expression> left;                                                                            \
        std::unique_ptr<Expression> right;                                                                           \
        std::string toString() const override;                                                                       \
        void buildScope(Scope& parentScope) override;                                                                \
    };

OSCAD_BINARY_OP(AdditionOp)
OSCAD_BINARY_OP(SubtractionOp)
OSCAD_BINARY_OP(MultiplicationOp)
OSCAD_BINARY_OP(DivisionOp)
OSCAD_BINARY_OP(ModuloOp)
OSCAD_BINARY_OP(ExponentOp)
OSCAD_BINARY_OP(BitwiseAndOp)
OSCAD_BINARY_OP(BitwiseOrOp)
OSCAD_BINARY_OP(BitwiseShiftLeftOp)
OSCAD_BINARY_OP(BitwiseShiftRightOp)
OSCAD_BINARY_OP(LogicalAndOp)
OSCAD_BINARY_OP(LogicalOrOp)
OSCAD_BINARY_OP(EqualityOp)
OSCAD_BINARY_OP(InequalityOp)
OSCAD_BINARY_OP(GreaterThanOp)
OSCAD_BINARY_OP(GreaterThanOrEqualOp)
OSCAD_BINARY_OP(LessThanOp)
OSCAD_BINARY_OP(LessThanOrEqualOp)

#undef OSCAD_BINARY_OP

class TernaryOp : public Expression {
public:
    TernaryOp(Position position, std::unique_ptr<Expression> condition, std::unique_ptr<Expression> trueExpr,
              std::unique_ptr<Expression> falseExpr)
        : Expression(NodeKind::TernaryOp, std::move(position)), condition(std::move(condition)),
          trueExpr(std::move(trueExpr)), falseExpr(std::move(falseExpr)) {}

    std::unique_ptr<Expression> condition;
    std::unique_ptr<Expression> trueExpr;
    std::unique_ptr<Expression> falseExpr;

    std::string toString() const override;
    void buildScope(Scope& parentScope) override;
};

// -- Postfix expressions -------------------------------------------------

class PrimaryCall : public Expression {
public:
    PrimaryCall(Position position, std::unique_ptr<Expression> left, std::vector<std::unique_ptr<Argument>> arguments)
        : Expression(NodeKind::PrimaryCall, std::move(position)), left(std::move(left)), arguments(std::move(arguments)) {}

    std::unique_ptr<Expression> left;
    std::vector<std::unique_ptr<Argument>> arguments;

    std::string toString() const override;
    void buildScope(Scope& parentScope) override;
};

class PrimaryIndex : public Expression {
public:
    PrimaryIndex(Position position, std::unique_ptr<Expression> left, std::unique_ptr<Expression> index)
        : Expression(NodeKind::PrimaryIndex, std::move(position)), left(std::move(left)), index(std::move(index)) {}

    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> index;

    std::string toString() const override { return left->toString() + "[" + index->toString() + "]"; }
    void buildScope(Scope& parentScope) override;
};

class PrimaryMember : public Expression {
public:
    PrimaryMember(Position position, std::unique_ptr<Expression> left, std::unique_ptr<Identifier> member)
        : Expression(NodeKind::PrimaryMember, std::move(position)), left(std::move(left)), member(std::move(member)) {}

    std::unique_ptr<Expression> left;
    std::unique_ptr<Identifier> member;

    std::string toString() const override { return left->toString() + "." + member->name; }
    void buildScope(Scope& parentScope) override;
};

} // namespace oscad
