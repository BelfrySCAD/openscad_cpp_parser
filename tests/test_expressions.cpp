// Ported from openscad_lalr_parser/tests/test_expressions.py.
#include "test_helpers.hpp"

#include <gtest/gtest.h>

using namespace oscad;

namespace {
std::vector<std::unique_ptr<ASTNode>> gAst;
Expression* E(const std::string& code) {
    return exprSrc(code, gAst);
}
} // namespace

// -- Arithmetic ops -----------------------------------------------------

TEST(Expr_Arithmetic, Addition) {
    auto* e = dynamic_cast<AdditionOp*>(E("1 + 2"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<NumberLiteral*>(e->left.get()), nullptr);
    EXPECT_NE(dynamic_cast<NumberLiteral*>(e->right.get()), nullptr);
}
TEST(Expr_Arithmetic, Subtraction) { EXPECT_NE(dynamic_cast<SubtractionOp*>(E("5 - 3")), nullptr); }
TEST(Expr_Arithmetic, Multiplication) { EXPECT_NE(dynamic_cast<MultiplicationOp*>(E("2 * 3")), nullptr); }
TEST(Expr_Arithmetic, Division) { EXPECT_NE(dynamic_cast<DivisionOp*>(E("10 / 2")), nullptr); }
TEST(Expr_Arithmetic, Modulo) { EXPECT_NE(dynamic_cast<ModuloOp*>(E("10 % 3")), nullptr); }
TEST(Expr_Arithmetic, Exponent) { EXPECT_NE(dynamic_cast<ExponentOp*>(E("2 ^ 3")), nullptr); }
TEST(Expr_Arithmetic, UnaryMinus) { EXPECT_NE(dynamic_cast<UnaryMinusOp*>(E("-5")), nullptr); }
TEST(Expr_Arithmetic, AdditionStr) { EXPECT_EQ(E("1 + 2")->toString(), "1 + 2"); }
TEST(Expr_Arithmetic, SubtractionStr) { EXPECT_EQ(E("5 - 3")->toString(), "5 - 3"); }

TEST(Expr_Arithmetic, ChainedOperationsLeftAssoc) {
    auto* e = dynamic_cast<AdditionOp*>(E("1 + 2 + 3"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<AdditionOp*>(e->left.get()), nullptr);
}

TEST(Expr_Arithmetic, MixedOperationsPrecedence) {
    auto* e = dynamic_cast<AdditionOp*>(E("1 + 2 * 3"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<MultiplicationOp*>(e->right.get()), nullptr);
}

// -- Precedence -----------------------------------------------------------

TEST(Expr_Precedence, MulBeforeAdd) {
    auto* e = dynamic_cast<AdditionOp*>(E("1 + 2 * 3"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<MultiplicationOp*>(e->right.get()), nullptr);
}
TEST(Expr_Precedence, ParenOverride) {
    auto* e = dynamic_cast<MultiplicationOp*>(E("(1 + 2) * 3"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<AdditionOp*>(e->left.get()), nullptr);
}
TEST(Expr_Precedence, LeftAssociativity) {
    auto* e = dynamic_cast<SubtractionOp*>(E("1 - 2 - 3"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<SubtractionOp*>(e->left.get()), nullptr);
}
TEST(Expr_Precedence, RightAssociativityExponent) {
    auto* e = dynamic_cast<ExponentOp*>(E("2 ^ 3 ^ 4"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<ExponentOp*>(e->right.get()), nullptr);
}
TEST(Expr_Precedence, ExponentiationBeforeMultiplication) {
    auto* e = dynamic_cast<MultiplicationOp*>(E("2 * 3 ^ 2"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<ExponentOp*>(e->right.get()), nullptr);
}
TEST(Expr_Precedence, NestedParentheses) {
    auto* e = dynamic_cast<DivisionOp*>(E("((1 + 2) * 3) / 4"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<MultiplicationOp*>(e->left.get()), nullptr);
}
TEST(Expr_Precedence, UnaryBindsTighterThanMul) {
    auto* e = dynamic_cast<MultiplicationOp*>(E("-2 * 3"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<UnaryMinusOp*>(e->left.get()), nullptr);
}

// -- Comparison ---------------------------------------------------------

TEST(Expr_Comparison, EqualityIdent) { EXPECT_NE(dynamic_cast<EqualityOp*>(E("a == b")), nullptr); }
TEST(Expr_Comparison, InequalityIdent) { EXPECT_NE(dynamic_cast<InequalityOp*>(E("a != b")), nullptr); }
TEST(Expr_Comparison, GreaterThanIdent) { EXPECT_NE(dynamic_cast<GreaterThanOp*>(E("a > b")), nullptr); }
TEST(Expr_Comparison, GreaterThanOrEqualIdent) { EXPECT_NE(dynamic_cast<GreaterThanOrEqualOp*>(E("a >= b")), nullptr); }
TEST(Expr_Comparison, LessThanIdent) { EXPECT_NE(dynamic_cast<LessThanOp*>(E("a < b")), nullptr); }
TEST(Expr_Comparison, LessThanOrEqualIdent) { EXPECT_NE(dynamic_cast<LessThanOrEqualOp*>(E("a <= b")), nullptr); }
TEST(Expr_Comparison, LessEqualNumeric) { EXPECT_NE(dynamic_cast<LessThanOrEqualOp*>(E("1 <= 2")), nullptr); }
TEST(Expr_Comparison, GreaterEqualNumeric) { EXPECT_NE(dynamic_cast<GreaterThanOrEqualOp*>(E("2 >= 1")), nullptr); }
TEST(Expr_Comparison, EqualNumeric) { EXPECT_NE(dynamic_cast<EqualityOp*>(E("1 == 2")), nullptr); }
TEST(Expr_Comparison, NotEqualNumeric) { EXPECT_NE(dynamic_cast<InequalityOp*>(E("1 != 2")), nullptr); }

// -- Logical --------------------------------------------------------------

TEST(Expr_Logical, And) { EXPECT_NE(dynamic_cast<LogicalAndOp*>(E("a && b")), nullptr); }
TEST(Expr_Logical, Or) { EXPECT_NE(dynamic_cast<LogicalOrOp*>(E("a || b")), nullptr); }
TEST(Expr_Logical, Not) { EXPECT_NE(dynamic_cast<LogicalNotOp*>(E("!a")), nullptr); }
TEST(Expr_Logical, AndOrPrecedence) {
    auto* e = dynamic_cast<LogicalOrOp*>(E("a || b && c"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<LogicalAndOp*>(e->right.get()), nullptr);
}
TEST(Expr_Logical, LogicalPrecedence) {
    auto* e = dynamic_cast<LogicalOrOp*>(E("true && false || true"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<LogicalAndOp*>(e->left.get()), nullptr);
}

// -- Bitwise --------------------------------------------------------------

TEST(Expr_Bitwise, And) { EXPECT_NE(dynamic_cast<BitwiseAndOp*>(E("5 & 3")), nullptr); }
TEST(Expr_Bitwise, Or) { EXPECT_NE(dynamic_cast<BitwiseOrOp*>(E("5 | 3")), nullptr); }
TEST(Expr_Bitwise, Not) { EXPECT_NE(dynamic_cast<BitwiseNotOp*>(E("~5")), nullptr); }
TEST(Expr_Bitwise, ShiftLeft) { EXPECT_NE(dynamic_cast<BitwiseShiftLeftOp*>(E("1 << 3")), nullptr); }
TEST(Expr_Bitwise, ShiftRight) { EXPECT_NE(dynamic_cast<BitwiseShiftRightOp*>(E("8 >> 2")), nullptr); }
TEST(Expr_Bitwise, BinaryShiftLeft) { EXPECT_NE(dynamic_cast<BitwiseShiftLeftOp*>(E("1 << 2")), nullptr); }

TEST(Expr_Bitwise, ShiftLeftChained) {
    auto* e = dynamic_cast<BitwiseShiftLeftOp*>(E("1 << 2 << 3"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<BitwiseShiftLeftOp*>(e->left.get()), nullptr);
}
TEST(Expr_Bitwise, ShiftRightChained) {
    auto* e = dynamic_cast<BitwiseShiftRightOp*>(E("64 >> 2 >> 1"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<BitwiseShiftRightOp*>(e->left.get()), nullptr);
}
TEST(Expr_Bitwise, ShiftMixedLeftToRight) {
    auto* e = dynamic_cast<BitwiseShiftRightOp*>(E("1 << 2 >> 1"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<BitwiseShiftLeftOp*>(e->left.get()), nullptr);
}
TEST(Expr_Bitwise, ShiftWithArithmetic) {
    auto* e = dynamic_cast<AdditionOp*>(E("(1 << 2) + 3"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<BitwiseShiftLeftOp*>(e->left.get()), nullptr);
}
TEST(Expr_Bitwise, ShiftInExpressionIdents) {
    auto* e = dynamic_cast<BitwiseShiftRightOp*>(E("a << b >> c"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<BitwiseShiftLeftOp*>(e->left.get()), nullptr);
}
TEST(Expr_Bitwise, NotWithExpression) {
    auto* e = dynamic_cast<BitwiseNotOp*>(E("~(1 + 2)"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<AdditionOp*>(e->expr.get()), nullptr);
}
TEST(Expr_Bitwise, NotChained) {
    auto* e = dynamic_cast<BitwiseNotOp*>(E("~~5"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<BitwiseNotOp*>(e->expr.get()), nullptr);
}
TEST(Expr_Bitwise, NotWithShift) {
    auto* e = dynamic_cast<BitwiseNotOp*>(E("~(1 << 2)"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<BitwiseShiftLeftOp*>(e->expr.get()), nullptr);
}

// -- Ternary ----------------------------------------------------------

TEST(Expr_Ternary, Basic) {
    auto* e = dynamic_cast<TernaryOp*>(E("a > 0 ? a : -a"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<GreaterThanOp*>(e->condition.get()), nullptr);
}
TEST(Expr_Ternary, Str) {
    std::string s = E("a > 0 ? a : b")->toString();
    EXPECT_NE(s.find('?'), std::string::npos);
    EXPECT_NE(s.find(':'), std::string::npos);
}
TEST(Expr_Ternary, Simple) {
    auto* e = dynamic_cast<TernaryOp*>(E("true ? 1 : 2"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<BooleanLiteral*>(e->condition.get()), nullptr);
}
TEST(Expr_Ternary, Nested) {
    auto* e = dynamic_cast<TernaryOp*>(E("true ? (false ? 1 : 2) : 3"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<TernaryOp*>(e->trueExpr.get()), nullptr);
}
TEST(Expr_Ternary, InExpression) {
    auto* e = dynamic_cast<MultiplicationOp*>(E("(true ? 1 : 2) * 3"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<TernaryOp*>(e->left.get()), nullptr);
}

// -- Postfix --------------------------------------------------------------

TEST(Expr_Postfix, FunctionCall) {
    auto* e = dynamic_cast<PrimaryCall*>(E("foo(1, 2)"));
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(dynamic_cast<Identifier*>(e->left.get())->name, "foo");
    EXPECT_EQ(e->arguments.size(), 2u);
}
TEST(Expr_Postfix, IndexAccess) {
    auto* e = dynamic_cast<PrimaryIndex*>(E("arr[0]"));
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(dynamic_cast<Identifier*>(e->left.get())->name, "arr");
}
TEST(Expr_Postfix, MemberAccess) {
    auto* e = dynamic_cast<PrimaryMember*>(E("obj.member"));
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->member->name, "member");
}
TEST(Expr_Postfix, ChainedCalls) {
    auto* e = dynamic_cast<PrimaryCall*>(E("foo(1)(2)"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<PrimaryCall*>(e->left.get()), nullptr);
}
TEST(Expr_Postfix, ChainedIndex) {
    auto* e = dynamic_cast<PrimaryIndex*>(E("arr[0][1]"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<PrimaryIndex*>(e->left.get()), nullptr);
}
TEST(Expr_Postfix, MemberChained) {
    auto* e = dynamic_cast<PrimaryMember*>(E("obj.member.submember"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<PrimaryMember*>(e->left.get()), nullptr);
}
TEST(Expr_Postfix, NoArgs) {
    auto* e = dynamic_cast<PrimaryCall*>(E("foo()"));
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->arguments.size(), 0u);
}
TEST(Expr_Postfix, NestedFunctionCalls) {
    auto* e = dynamic_cast<PrimaryCall*>(E("sin(cos(0))"));
    ASSERT_NE(e, nullptr);
    auto* arg0 = dynamic_cast<PositionalArgument*>(e->arguments[0].get());
    ASSERT_NE(arg0, nullptr);
    EXPECT_NE(dynamic_cast<PrimaryCall*>(arg0->expr.get()), nullptr);
}
TEST(Expr_Postfix, CallInExpression) {
    EXPECT_NE(dynamic_cast<AdditionOp*>(E("sin(0) + cos(0)")), nullptr);
}
TEST(Expr_Postfix, MemberAccessInExpression) {
    auto* e = dynamic_cast<AdditionOp*>(E("obj.member + 1"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<PrimaryMember*>(e->left.get()), nullptr);
}
TEST(Expr_Postfix, ArrayAccessExpression) {
    auto* e = dynamic_cast<PrimaryIndex*>(E("arr[i + 1]"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<AdditionOp*>(e->index.get()), nullptr);
}
TEST(Expr_Postfix, CallWithMember) {
    auto* e = dynamic_cast<PrimaryCall*>(E("obj.func(1, 2)"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<PrimaryMember*>(e->left.get()), nullptr);
}

// -- Unary ----------------------------------------------------------------

TEST(Expr_Unary, PlusIsPassThrough) {
    auto* n = dynamic_cast<NumberLiteral*>(E("+5"));
    ASSERT_NE(n, nullptr);
}
TEST(Expr_Unary, Not) { EXPECT_NE(dynamic_cast<LogicalNotOp*>(E("!true")), nullptr); }

// -- Complex expressions -------------------------------------------------

TEST(Expr_Complex, Case1) { EXPECT_NE(dynamic_cast<DivisionOp*>(E("(a + b) * (c - d) / (e % f)")), nullptr); }
TEST(Expr_Complex, Case2) { EXPECT_NE(dynamic_cast<LogicalOrOp*>(E("a > b && c < d || e == f")), nullptr); }
TEST(Expr_Complex, Case3) { EXPECT_NE(dynamic_cast<AdditionOp*>(E("sin(a) * cos(b) + tan(c)")), nullptr); }
TEST(Expr_Complex, Case4) { EXPECT_NE(dynamic_cast<AdditionOp*>(E("arr[i] + arr[j] * arr[k]")), nullptr); }
TEST(Expr_Complex, Case5) { EXPECT_NE(dynamic_cast<TernaryOp*>(E("a > b ? c + d : e - f")), nullptr); }
TEST(Expr_Complex, MultipleUnary) {
    auto* e = dynamic_cast<UnaryMinusOp*>(E("--5"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<UnaryMinusOp*>(e->expr.get()), nullptr);
}
TEST(Expr_Complex, UnaryWithExpression) {
    auto* e = dynamic_cast<UnaryMinusOp*>(E("-(1 + 2)"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<AdditionOp*>(e->expr.get()), nullptr);
}
