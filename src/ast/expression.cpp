#include "openscad_cpp_parser/ast/expression.hpp"

#include "format_utils.hpp"
#include "openscad_cpp_parser/ast/scope_builder.hpp"
#include "openscad_cpp_parser/scope.hpp"

#include <array>
#include <charconv>

namespace oscad {

// ponytail: uses std::to_chars' shortest round-trip formatting rather than
// replicating CPython's dtoa exponent-threshold/padding rules exactly.
// Matches Python's str(float) for all integer and typical decimal values
// (the vast majority of real .scad source); may differ cosmetically at
// extreme magnitudes. Exact OpenSCAD-source-formatting fidelity lives in
// pretty_print.cpp's toOpenscad(), not here -- this is a debug repr.
std::string NumberLiteral::toString() const {
    std::array<char, 64> buf{};
    auto result = std::to_chars(buf.data(), buf.data() + buf.size(), val);
    return std::string(buf.data(), result.ptr);
}

std::string RangeLiteral::toString() const {
    // Print back the form that was written. Emitting the synthesized step for
    // a two-argument range would round-trip `[5:0]` into `[5 : 1 : 0]`, which
    // reads identically but suppresses the evaluator's backwards-range warning.
    if (implicitStep) return "[" + start->toString() + " : " + end->toString() + "]";
    return "[" + start->toString() + " : " + step->toString() + " : " + end->toString() + "]";
}

void RangeLiteral::buildScope(Scope& parentScope) {
    setScope(parentScope);
    start->buildScope(parentScope);
    end->buildScope(parentScope);
    step->buildScope(parentScope);
}

void PositionalArgument::buildScope(Scope& parentScope) {
    setScope(parentScope);
    expr->buildScope(parentScope);
}

void NamedArgument::buildScope(Scope& parentScope) {
    setScope(parentScope);
    name->buildScope(parentScope);
    expr->buildScope(parentScope);
}

std::string ParameterDeclaration::toString() const {
    bool hasDefault = defaultValue && defaultValue->kind() != NodeKind::UndefinedLiteral;
    return name->name + (hasDefault ? ("=" + defaultValue->toString()) : "");
}

void ParameterDeclaration::buildScope(Scope& parentScope) {
    setScope(parentScope);
    for (auto& c : leadingComments) {
        c->buildScope(parentScope);
    }
    name->buildScope(parentScope);
    for (auto& c : trailingComments) {
        c->buildScope(parentScope);
    }
    if (defaultValue) {
        // Defaults are evaluated in the definition site's ENCLOSING scope,
        // not the function/module's own new scope.
        Scope& callerScope = parentScope.parent() ? *parentScope.parent() : parentScope;
        defaultValue->buildScope(callerScope);
    }
}

void Assignment::buildScope(Scope& parentScope) {
    setScope(parentScope);
    name->buildScope(parentScope);
    expr->buildScope(parentScope);
}

void Assignment::buildScopeSplit(Scope& nameScope, Scope& exprScope) {
    setScope(nameScope);
    name->buildScope(nameScope);
    expr->buildScope(exprScope);
}

std::string CommentedExpr::toString() const {
    std::string parts;
    for (const auto& c : leadingComments) {
        parts += c->toString() + " ";
    }
    parts += expr->toString();
    for (const auto& c : trailingComments) {
        parts += " " + c->toString();
    }
    return parts;
}

void CommentedExpr::buildScope(Scope& parentScope) {
    setScope(parentScope);
    for (auto& c : leadingComments) {
        c->buildScope(parentScope);
    }
    for (auto& c : trailingComments) {
        c->buildScope(parentScope);
    }
    expr->buildScope(parentScope);
}

std::string LetOp::toString() const {
    return "let(" + joinToString(assignments, ", ") + ") " + body->toString();
}

void LetOp::buildScope(Scope& parentScope) {
    setScope(parentScope);
    Scope& letScope = parentScope.childScope();
    // Sequential and self-referential: the name is defined in letScope
    // BEFORE that same assignment's own buildScope() runs, so a lookup of
    // the name inside its own RHS resolves to itself, not an outer binding.
    // Later assignments see earlier ones. Replicate literally.
    for (auto& a : assignments) {
        letScope.defineVariable(a->name->name, a.get());
        a->buildScope(letScope);
    }
    body->buildScope(letScope);
}

std::string EchoOp::toString() const {
    return "echo(" + joinToString(arguments, ", ") + ") " + body->toString();
}

void EchoOp::buildScope(Scope& parentScope) {
    setScope(parentScope);
    for (auto& a : arguments) {
        a->buildScope(parentScope);
    }
    body->buildScope(parentScope);
}

std::string AssertOp::toString() const {
    return "assert(" + joinToString(arguments, ", ") + ") " + body->toString();
}

void AssertOp::buildScope(Scope& parentScope) {
    setScope(parentScope);
    for (auto& a : arguments) {
        a->buildScope(parentScope);
    }
    body->buildScope(parentScope);
}

std::string FunctionLiteral::toString() const {
    return "function(" + joinToString(parameters, ", ") + ") " + body->toString();
}

void FunctionLiteral::buildScope(Scope& parentScope) {
    setScope(parentScope);
    Scope& funcScope = parentScope.childScope();
    for (auto& p : parameters) {
        funcScope.defineVariable(p->name->name, p.get());
        p->buildScope(funcScope);
    }
    body->buildScope(funcScope);
}

std::string RenderExpression::toString() const {
    // Deliberately NOT ModuleInstantiation's formatChildBlock, which omits
    // both braces (for a lone child) and every statement terminator, because
    // for a STATEMENT the terminators come from the statement-level printer.
    // A RenderExpression sits inside an expression, where nothing downstream
    // adds them -- and `render() cube(1)` unbraced is precisely the form that
    // does NOT parse (the child_statement swallows the `;`, leaving the
    // enclosing assignment unterminated). So: always braces, always a `;`
    // after every child.
    //
    // This must stay REPARSEABLE -- pretty_print.cpp's fmtExpr falls through
    // to toString() for expression kinds it has no case for, so this string
    // is what a formatter emits. NodeStr.RenderExpressionRoundTrips guards it.
    //
    // The unconditional `;` is safe even after a child that already ends in
    // `}` (a nested block, a module declaration): a bare `;` is itself a legal
    // statement (parser.y's `statement: ";"`), so a redundant one is a no-op.
    std::string s = "render(" + joinToString(arguments, ", ") + ") { ";
    for (const auto& c : children) {
        s += c->toString() + "; ";
    }
    return s + "}";
}

void RenderExpression::buildScope(Scope& parentScope) {
    // Same shape as ModularCall::buildScope minus the name lookup (there is
    // no Identifier -- "render" is a keyword token here). The hoist is
    // required: `render() { x = 1; cube(x); }` must resolve x.
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

// -- Operator precedence for minimal-parenthesization toString() ----------
//
// Matches the reference's nodes.py::_PREC/_lp/_rp: toString() adds parens
// only where needed to preserve the parse, not unconditionally. Precedence
// values mirror the bison grammar's cascade (parser.y) -- lower binds
// looser. 0 is reserved for expression forms (TernaryOp/LetOp/EchoOp/
// AssertOp/FunctionLiteral) that are only ever reachable unparenthesized
// as the WHOLE of `expr`, never as an operand of a tighter operator --so
// they always need parens when nested inside one. 100 is primary/postfix
// forms, which never need parens as an operand of anything.
namespace {

int operatorPrecedence(NodeKind kind) {
    switch (kind) {
        case NodeKind::TernaryOp:
        case NodeKind::LetOp:
        case NodeKind::EchoOp:
        case NodeKind::AssertOp:
        case NodeKind::FunctionLiteral:
            return 0;
        case NodeKind::LogicalOrOp:
            return 1;
        case NodeKind::LogicalAndOp:
            return 2;
        case NodeKind::EqualityOp:
        case NodeKind::InequalityOp:
            return 3;
        case NodeKind::LessThanOp:
        case NodeKind::GreaterThanOp:
        case NodeKind::LessThanOrEqualOp:
        case NodeKind::GreaterThanOrEqualOp:
            return 4;
        case NodeKind::BitwiseOrOp:
            return 5;
        case NodeKind::BitwiseAndOp:
            return 6;
        case NodeKind::BitwiseShiftLeftOp:
        case NodeKind::BitwiseShiftRightOp:
            return 7;
        case NodeKind::AdditionOp:
        case NodeKind::SubtractionOp:
            return 8;
        case NodeKind::MultiplicationOp:
        case NodeKind::DivisionOp:
        case NodeKind::ModuloOp:
            return 9;
        case NodeKind::UnaryMinusOp:
        case NodeKind::LogicalNotOp:
        case NodeKind::BitwiseNotOp:
            return 10;
        case NodeKind::ExponentOp:
            return 11;
        default:
            return 100;
    }
}

std::string parenthesizeIf(const Expression& child, bool needsParens) {
    return needsParens ? "(" + child.toString() + ")" : child.toString();
}

// Left-associative binary op at precedence `prec`: left operand only needs
// parens if strictly looser; right operand also needs parens if EQUAL
// precedence (since "1 - (2 - 3)" would otherwise reprint as "1 - 2 - 3",
// changing meaning).
std::string fmtLeftAssocLeft(const Expression& child, int prec) {
    return parenthesizeIf(child, operatorPrecedence(child.kind()) < prec);
}
std::string fmtLeftAssocRight(const Expression& child, int prec) {
    return parenthesizeIf(child, operatorPrecedence(child.kind()) <= prec);
}

// Right-associative (Exponent only): mirror image of the above.
std::string fmtRightAssocLeft(const Expression& child, int prec) {
    return parenthesizeIf(child, operatorPrecedence(child.kind()) <= prec);
}
std::string fmtRightAssocRight(const Expression& child, int prec) {
    return parenthesizeIf(child, operatorPrecedence(child.kind()) < prec);
}

std::string fmtUnaryOperand(const Expression& child, int prec) {
    return parenthesizeIf(child, operatorPrecedence(child.kind()) < prec);
}

} // namespace

#define OSCAD_UNARY_OP_IMPL(ClassName, OpStr, Prec)                                                                    \
    std::string ClassName::toString() const { return OpStr + fmtUnaryOperand(*expr, Prec); }                          \
    void ClassName::buildScope(Scope& parentScope) {                                                                  \
        setScope(parentScope);                                                                                        \
        expr->buildScope(parentScope);                                                                                \
    }

OSCAD_UNARY_OP_IMPL(UnaryMinusOp, "-", 10)
OSCAD_UNARY_OP_IMPL(LogicalNotOp, "!", 10)
OSCAD_UNARY_OP_IMPL(BitwiseNotOp, "~", 10)

#undef OSCAD_UNARY_OP_IMPL

// -- Binary operators (left-associative, except ExponentOp below) ---------

#define OSCAD_BINARY_OP_IMPL(ClassName, OpStr, Prec)                                                                   \
    std::string ClassName::toString() const {                                                                         \
        return fmtLeftAssocLeft(*left, Prec) + " " OpStr " " + fmtLeftAssocRight(*right, Prec);                       \
    }                                                                                                                   \
    void ClassName::buildScope(Scope& parentScope) {                                                                  \
        setScope(parentScope);                                                                                        \
        left->buildScope(parentScope);                                                                                \
        right->buildScope(parentScope);                                                                               \
    }

OSCAD_BINARY_OP_IMPL(AdditionOp, "+", 8)
OSCAD_BINARY_OP_IMPL(SubtractionOp, "-", 8)
OSCAD_BINARY_OP_IMPL(MultiplicationOp, "*", 9)
OSCAD_BINARY_OP_IMPL(DivisionOp, "/", 9)
OSCAD_BINARY_OP_IMPL(ModuloOp, "%", 9)
OSCAD_BINARY_OP_IMPL(BitwiseAndOp, "&", 6)
OSCAD_BINARY_OP_IMPL(BitwiseOrOp, "|", 5)
OSCAD_BINARY_OP_IMPL(BitwiseShiftLeftOp, "<<", 7)
OSCAD_BINARY_OP_IMPL(BitwiseShiftRightOp, ">>", 7)
OSCAD_BINARY_OP_IMPL(LogicalAndOp, "&&", 2)
OSCAD_BINARY_OP_IMPL(LogicalOrOp, "||", 1)
OSCAD_BINARY_OP_IMPL(EqualityOp, "==", 3)
OSCAD_BINARY_OP_IMPL(InequalityOp, "!=", 3)
OSCAD_BINARY_OP_IMPL(GreaterThanOp, ">", 4)
OSCAD_BINARY_OP_IMPL(GreaterThanOrEqualOp, ">=", 4)
OSCAD_BINARY_OP_IMPL(LessThanOp, "<", 4)
OSCAD_BINARY_OP_IMPL(LessThanOrEqualOp, "<=", 4)

#undef OSCAD_BINARY_OP_IMPL

std::string ExponentOp::toString() const {
    return fmtRightAssocLeft(*left, 11) + " ^ " + fmtRightAssocRight(*right, 11);
}

void ExponentOp::buildScope(Scope& parentScope) {
    setScope(parentScope);
    left->buildScope(parentScope);
    right->buildScope(parentScope);
}

std::string TernaryOp::toString() const {
    return condition->toString() + " ? " + trueExpr->toString() + " : " + falseExpr->toString();
}

void TernaryOp::buildScope(Scope& parentScope) {
    setScope(parentScope);
    condition->buildScope(parentScope);
    trueExpr->buildScope(parentScope);
    falseExpr->buildScope(parentScope);
}

// -- Postfix expressions ---------------------------------------------------

std::string PrimaryCall::toString() const {
    return left->toString() + "(" + joinToString(arguments, ", ") + ")";
}

void PrimaryCall::buildScope(Scope& parentScope) {
    setScope(parentScope);
    left->buildScope(parentScope);
    for (auto& a : arguments) {
        a->buildScope(parentScope);
    }
}

void PrimaryIndex::buildScope(Scope& parentScope) {
    setScope(parentScope);
    left->buildScope(parentScope);
    index->buildScope(parentScope);
}

void PrimaryMember::buildScope(Scope& parentScope) {
    setScope(parentScope);
    left->buildScope(parentScope);
    member->buildScope(parentScope);
}

} // namespace oscad
