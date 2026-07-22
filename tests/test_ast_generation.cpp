// Ported from openscad_lalr_parser/tests/test_ast_generation.py.
//
// This file heavily overlaps with test_expressions.py, test_vectors.py,
// test_modules.py, test_control.py, and test_functions.py (many identical
// test names/cases). Rather than duplicate those, this file ports only the
// coverage that's genuinely NEW here: source-position tracking, detailed
// logical/bitwise-not precedence, argument/parameter/element COUNT SWEEPS
// (0-3 patterns for PrimaryCall/ModularCall/EchoOp/ModularEcho/
// ModularAssert/ListComprehension/LetOp not exercised as systematically
// elsewhere), and multi-node integration scenarios.
#include "test_helpers.hpp"

#include <gtest/gtest.h>

using namespace oscad;

namespace {
std::vector<std::unique_ptr<ASTNode>> gAst;
Expression* E(const std::string& code) {
    return exprSrc(code, gAst);
}
} // namespace

// -- Position tracking --------------------------------------------------

TEST(PositionASTNodes, DefaultOrigin) {
    auto ast = parseSrc("x = 42;");
    EXPECT_EQ(ast[0]->position().origin, "<string>");
    EXPECT_GE(ast[0]->position().line, 1);
    EXPECT_GE(ast[0]->position().column, 1);
}

TEST(PositionASTNodes, CustomOrigin) {
    auto ast = parseAst("x = 42;", "test.scad");
    EXPECT_EQ(ast[0]->position().origin, "test.scad");
}

TEST(PositionASTNodes, LineColumn) {
    auto ast = parseSrc("x = 42;");
    EXPECT_EQ(ast[0]->position().line, 1);
    EXPECT_GE(ast[0]->position().column, 1);
}

TEST(PositionASTNodes, Multiline) {
    auto ast = parseSrc("x = 1;\ny = 2;");
    EXPECT_EQ(ast[0]->position().line, 1);
    EXPECT_EQ(ast[1]->position().line, 2);
}

TEST(PositionASTNodes, PreservesAcrossExpressions) {
    auto ast = parseSrc("x = 1 + 2;");
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    // Position is a value member, always present; this just confirms the
    // expr's own span was populated (non-default), matching the source.
    EXPECT_GE(a->expr->position().line, 1);
}

TEST(PositionASTNodes, OnModuleDeclaration) {
    auto ast = parseSrc("module foo() { cube(1); }");
    EXPECT_EQ(ast[0]->position().line, 1);
}

TEST(PositionASTNodes, OnFunctionDeclaration) {
    auto ast = parseSrc("function add(a, b) = a + b;");
    EXPECT_EQ(ast[0]->position().line, 1);
}

TEST(PositionASTNodes, OnModularCall) {
    auto ast = parseSrc("cube(10);");
    EXPECT_GE(ast[0]->position().line, 1);
}

TEST(PositionASTNodes, OriginAppliesToAllNodes) {
    auto ast = parseAst("x = 1;\ny = 2;", "test.scad");
    for (auto& node : ast) {
        EXPECT_EQ(node->position().origin, "test.scad");
    }
}

// -- Complex integration scenarios ---------------------------------------

TEST(ComplexASTNodes, NestedArithmetic) {
    auto* e = dynamic_cast<MultiplicationOp*>(E("(a + b) * (c - d)"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<AdditionOp*>(e->left.get()), nullptr);
    EXPECT_NE(dynamic_cast<SubtractionOp*>(e->right.get()), nullptr);
}

TEST(ComplexASTNodes, DeeplyNestedExpression) {
    auto* e = dynamic_cast<AdditionOp*>(E("a + b * c + d"));
    ASSERT_NE(e, nullptr);
    auto* inner = dynamic_cast<AdditionOp*>(e->left.get());
    ASSERT_NE(inner, nullptr);
    EXPECT_NE(dynamic_cast<MultiplicationOp*>(inner->right.get()), nullptr);
}

TEST(ComplexASTNodes, ChainedFunctionCalls) {
    auto* e = dynamic_cast<PrimaryCall*>(E("foo(1)(2)(3)"));
    ASSERT_NE(e, nullptr);
    auto* mid = dynamic_cast<PrimaryCall*>(e->left.get());
    ASSERT_NE(mid, nullptr);
    EXPECT_NE(dynamic_cast<PrimaryCall*>(mid->left.get()), nullptr);
}

TEST(ComplexASTNodes, ComplexModule) {
    auto ast = parseSrc("module box(size, center=true) {\n"
                         "    cube(size, center=center);\n"
                         "    translate([0, 0, size])\n"
                         "        sphere(r=size/2);\n"
                         "}");
    auto* m = dynamic_cast<ModuleDeclaration*>(ast[0].get());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->name->name, "box");
    EXPECT_EQ(m->parameters.size(), 2u);
    EXPECT_EQ(m->children.size(), 2u);
}

TEST(ComplexASTNodes, ComplexFunction) {
    auto ast = parseSrc("function dist(a, b) = sqrt((a[0]-b[0])^2 + (a[1]-b[1])^2);");
    auto* f = dynamic_cast<FunctionDeclaration*>(ast[0].get());
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->name->name, "dist");
    EXPECT_EQ(f->parameters.size(), 2u);
}

TEST(ComplexASTNodes, NestedModuleCalls) {
    auto ast = parseSrc("difference() { cube(10); translate([2,2,2]) cube(6); }");
    auto* c = dynamic_cast<ModularCall*>(ast[0].get());
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->name->name, "difference");
    EXPECT_EQ(c->children.size(), 2u);
}

TEST(ComplexASTNodes, ForWithIf) {
    auto ast = parseSrc("for (i=[0:10]) if (i > 5) cube(i);");
    auto* f = dynamic_cast<ModularFor*>(ast[0].get());
    ASSERT_NE(f, nullptr);
    ASSERT_EQ(f->body.size(), 1u);
    EXPECT_NE(dynamic_cast<ModularIf*>(f->body[0].get()), nullptr);
}

TEST(ComplexASTNodes, MixedStatements) {
    auto ast = parseSrc("use <BOSL2/std.scad>\n"
                         "x = 10;\n"
                         "module foo() { cube(x); }\n"
                         "function bar(a) = a * 2;\n"
                         "foo();");
    ASSERT_EQ(ast.size(), 5u);
    EXPECT_NE(dynamic_cast<UseStatement*>(ast[0].get()), nullptr);
    EXPECT_NE(dynamic_cast<Assignment*>(ast[1].get()), nullptr);
    EXPECT_NE(dynamic_cast<ModuleDeclaration*>(ast[2].get()), nullptr);
    EXPECT_NE(dynamic_cast<FunctionDeclaration*>(ast[3].get()), nullptr);
    EXPECT_NE(dynamic_cast<ModularCall*>(ast[4].get()), nullptr);
}

// -- Logical not (detailed precedence) ------------------------------------

TEST(LogicalNotAST, NotTrue) {
    auto* e = dynamic_cast<LogicalNotOp*>(E("!true"));
    ASSERT_NE(e, nullptr);
    auto* b = dynamic_cast<BooleanLiteral*>(e->expr.get());
    ASSERT_NE(b, nullptr);
    EXPECT_TRUE(b->val);
}
TEST(LogicalNotAST, NotFalse) {
    auto* e = dynamic_cast<LogicalNotOp*>(E("!false"));
    ASSERT_NE(e, nullptr);
    EXPECT_FALSE(dynamic_cast<BooleanLiteral*>(e->expr.get())->val);
}
TEST(LogicalNotAST, NotIdentifier) {
    auto* e = dynamic_cast<LogicalNotOp*>(E("!x"));
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(dynamic_cast<Identifier*>(e->expr.get())->name, "x");
}
TEST(LogicalNotAST, NotDouble) {
    auto* e = dynamic_cast<LogicalNotOp*>(E("!!x"));
    ASSERT_NE(e, nullptr);
    auto* inner = dynamic_cast<LogicalNotOp*>(e->expr.get());
    ASSERT_NE(inner, nullptr);
    EXPECT_NE(dynamic_cast<Identifier*>(inner->expr.get()), nullptr);
}
TEST(LogicalNotAST, NotEquality) {
    auto* e = dynamic_cast<LogicalNotOp*>(E("!(a == b)"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<EqualityOp*>(e->expr.get()), nullptr);
}
TEST(LogicalNotAST, NotInequality) {
    auto* e = dynamic_cast<LogicalNotOp*>(E("!(a != b)"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<InequalityOp*>(e->expr.get()), nullptr);
}
TEST(LogicalNotAST, NotComparison) {
    auto* e = dynamic_cast<LogicalNotOp*>(E("!(a > b)"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<GreaterThanOp*>(e->expr.get()), nullptr);
}
TEST(LogicalNotAST, NotBindsTighterThanAnd) {
    auto* e = dynamic_cast<LogicalAndOp*>(E("!a && b"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<LogicalNotOp*>(e->left.get()), nullptr);
    EXPECT_NE(dynamic_cast<Identifier*>(e->right.get()), nullptr);
}
TEST(LogicalNotAST, NotBindsTighterThanOr) {
    auto* e = dynamic_cast<LogicalOrOp*>(E("!a || b"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<LogicalNotOp*>(e->left.get()), nullptr);
}
TEST(LogicalNotAST, NotBothSidesOfAnd) {
    auto* e = dynamic_cast<LogicalAndOp*>(E("!a && !b"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<LogicalNotOp*>(e->left.get()), nullptr);
    EXPECT_NE(dynamic_cast<LogicalNotOp*>(e->right.get()), nullptr);
}
TEST(LogicalNotAST, NotInTernaryCondition) {
    auto* e = dynamic_cast<TernaryOp*>(E("!a ? b : c"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<LogicalNotOp*>(e->condition.get()), nullptr);
}
TEST(LogicalNotAST, NotInTernaryBranch) {
    auto* e = dynamic_cast<TernaryOp*>(E("a ? !b : !c"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<LogicalNotOp*>(e->trueExpr.get()), nullptr);
    EXPECT_NE(dynamic_cast<LogicalNotOp*>(e->falseExpr.get()), nullptr);
}
TEST(LogicalNotAST, NotInIfCondition) {
    auto ast = parseSrc("if (!x) cube(1);");
    auto* ifNode = dynamic_cast<ModularIf*>(ast[0].get());
    ASSERT_NE(ifNode, nullptr);
    EXPECT_NE(dynamic_cast<LogicalNotOp*>(ifNode->condition.get()), nullptr);
}
TEST(LogicalNotAST, StrSimple) { EXPECT_EQ(E("!x")->toString(), "!x"); }
TEST(LogicalNotAST, StrDouble) { EXPECT_EQ(E("!!x")->toString(), "!!x"); }

// -- Bitwise not (detailed precedence) ------------------------------------

TEST(BitwiseNotAST, NotNumber) {
    auto* e = dynamic_cast<BitwiseNotOp*>(E("~42"));
    ASSERT_NE(e, nullptr);
    EXPECT_DOUBLE_EQ(dynamic_cast<NumberLiteral*>(e->expr.get())->val, 42.0);
}
TEST(BitwiseNotAST, NotIdentifier) {
    auto* e = dynamic_cast<BitwiseNotOp*>(E("~x"));
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(dynamic_cast<Identifier*>(e->expr.get())->name, "x");
}
TEST(BitwiseNotAST, NotDouble) {
    auto* e = dynamic_cast<BitwiseNotOp*>(E("~~x"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<BitwiseNotOp*>(e->expr.get()), nullptr);
}
TEST(BitwiseNotAST, NotParenthesizedAddition) {
    auto* e = dynamic_cast<BitwiseNotOp*>(E("~(a + b)"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<AdditionOp*>(e->expr.get()), nullptr);
}
TEST(BitwiseNotAST, NotParenthesizedShift) {
    auto* e = dynamic_cast<BitwiseNotOp*>(E("~(a << b)"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<BitwiseShiftLeftOp*>(e->expr.get()), nullptr);
}
TEST(BitwiseNotAST, NotBindsTighterThanBitwiseAnd) {
    auto* e = dynamic_cast<BitwiseAndOp*>(E("~a & b"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<BitwiseNotOp*>(e->left.get()), nullptr);
}
TEST(BitwiseNotAST, NotBindsTighterThanBitwiseOr) {
    auto* e = dynamic_cast<BitwiseOrOp*>(E("~a | b"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<BitwiseNotOp*>(e->left.get()), nullptr);
}
TEST(BitwiseNotAST, NotBothSidesOfAnd) {
    auto* e = dynamic_cast<BitwiseAndOp*>(E("~a & ~b"));
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<BitwiseNotOp*>(e->left.get()), nullptr);
    EXPECT_NE(dynamic_cast<BitwiseNotOp*>(e->right.get()), nullptr);
}
TEST(BitwiseNotAST, StrSimple) { EXPECT_EQ(E("~x")->toString(), "~x"); }
TEST(BitwiseNotAST, StrDouble) { EXPECT_EQ(E("~~x")->toString(), "~~x"); }

// -- Mixed logical/bitwise not --------------------------------------------

TEST(MixedNotOperatorsAST, BitwiseNotOfLogicalNot) {
    auto* e = dynamic_cast<BitwiseNotOp*>(E("~!x"));
    ASSERT_NE(e, nullptr);
    auto* inner = dynamic_cast<LogicalNotOp*>(e->expr.get());
    ASSERT_NE(inner, nullptr);
    EXPECT_NE(dynamic_cast<Identifier*>(inner->expr.get()), nullptr);
}
TEST(MixedNotOperatorsAST, LogicalNotOfBitwiseNot) {
    auto* e = dynamic_cast<LogicalNotOp*>(E("!~x"));
    ASSERT_NE(e, nullptr);
    auto* inner = dynamic_cast<BitwiseNotOp*>(e->expr.get());
    ASSERT_NE(inner, nullptr);
    EXPECT_NE(dynamic_cast<Identifier*>(inner->expr.get()), nullptr);
}
TEST(MixedNotOperatorsAST, ThreeLevelsOfNesting) {
    auto* e = dynamic_cast<BitwiseNotOp*>(E("~!~x"));
    ASSERT_NE(e, nullptr);
    auto* mid = dynamic_cast<LogicalNotOp*>(e->expr.get());
    ASSERT_NE(mid, nullptr);
    EXPECT_NE(dynamic_cast<BitwiseNotOp*>(mid->expr.get()), nullptr);
}

// -- Argument/parameter/element count sweeps (0-3) -----------------------

TEST(PrimaryCallArguments, Zero) { EXPECT_EQ(dynamic_cast<PrimaryCall*>(E("foo()"))->arguments.size(), 0u); }
TEST(PrimaryCallArguments, OnePositional) {
    auto* c = dynamic_cast<PrimaryCall*>(E("foo(1)"));
    ASSERT_EQ(c->arguments.size(), 1u);
    EXPECT_NE(dynamic_cast<PositionalArgument*>(c->arguments[0].get()), nullptr);
}
TEST(PrimaryCallArguments, TwoPositional) { EXPECT_EQ(dynamic_cast<PrimaryCall*>(E("foo(1, 2)"))->arguments.size(), 2u); }
TEST(PrimaryCallArguments, ThreePositional) {
    EXPECT_EQ(dynamic_cast<PrimaryCall*>(E("foo(1, 2, 3)"))->arguments.size(), 3u);
}
TEST(PrimaryCallArguments, OneNamed) {
    auto* c = dynamic_cast<PrimaryCall*>(E("foo(a=1)"));
    ASSERT_EQ(c->arguments.size(), 1u);
    EXPECT_NE(dynamic_cast<NamedArgument*>(c->arguments[0].get()), nullptr);
}
TEST(PrimaryCallArguments, TwoNamed) {
    auto* c = dynamic_cast<PrimaryCall*>(E("foo(a=1, b=2)"));
    ASSERT_EQ(c->arguments.size(), 2u);
    EXPECT_NE(dynamic_cast<NamedArgument*>(c->arguments[0].get()), nullptr);
    EXPECT_NE(dynamic_cast<NamedArgument*>(c->arguments[1].get()), nullptr);
}
TEST(PrimaryCallArguments, MixedPositionalAndNamed) {
    auto* c = dynamic_cast<PrimaryCall*>(E("foo(1, a=2)"));
    ASSERT_EQ(c->arguments.size(), 2u);
    EXPECT_NE(dynamic_cast<PositionalArgument*>(c->arguments[0].get()), nullptr);
    EXPECT_NE(dynamic_cast<NamedArgument*>(c->arguments[1].get()), nullptr);
}

namespace {
ModularCall* callOf(const std::string& code, std::vector<std::unique_ptr<ASTNode>>& ast) {
    ast = parseSrc(code);
    return dynamic_cast<ModularCall*>(ast[0].get());
}
} // namespace

TEST(ModularCallArguments, Zero) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    EXPECT_EQ(callOf("foo();", ast)->arguments.size(), 0u);
}
TEST(ModularCallArguments, OnePositional) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    auto* c = callOf("foo(1);", ast);
    ASSERT_EQ(c->arguments.size(), 1u);
    EXPECT_NE(dynamic_cast<PositionalArgument*>(c->arguments[0].get()), nullptr);
}
TEST(ModularCallArguments, TwoPositional) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    EXPECT_EQ(callOf("foo(1, 2);", ast)->arguments.size(), 2u);
}
TEST(ModularCallArguments, ThreePositional) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    EXPECT_EQ(callOf("foo(1, 2, 3);", ast)->arguments.size(), 3u);
}
TEST(ModularCallArguments, OneNamed) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    auto* c = callOf("foo(a=1);", ast);
    ASSERT_EQ(c->arguments.size(), 1u);
    EXPECT_NE(dynamic_cast<NamedArgument*>(c->arguments[0].get()), nullptr);
}
TEST(ModularCallArguments, TwoNamed) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    EXPECT_EQ(callOf("foo(a=1, b=2);", ast)->arguments.size(), 2u);
}
TEST(ModularCallArguments, MixedPositionalAndNamed) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    auto* c = callOf("foo(1, a=2);", ast);
    ASSERT_EQ(c->arguments.size(), 2u);
    EXPECT_NE(dynamic_cast<PositionalArgument*>(c->arguments[0].get()), nullptr);
    EXPECT_NE(dynamic_cast<NamedArgument*>(c->arguments[1].get()), nullptr);
}

TEST(EchoArguments, EchoOpZeroToThree) {
    EXPECT_EQ(dynamic_cast<EchoOp*>(E("echo() x"))->arguments.size(), 0u);
    EXPECT_EQ(dynamic_cast<EchoOp*>(E("echo(\"a\") x"))->arguments.size(), 1u);
    EXPECT_EQ(dynamic_cast<EchoOp*>(E("echo(\"a\", \"b\") x"))->arguments.size(), 2u);
    EXPECT_EQ(dynamic_cast<EchoOp*>(E("echo(\"a\", \"b\", \"c\") x"))->arguments.size(), 3u);
}
TEST(EchoArguments, ModularEchoZeroToThree) {
    EXPECT_EQ(dynamic_cast<ModularEcho*>(parseSrc("echo() cube(1);")[0].get())->arguments.size(), 0u);
    EXPECT_EQ(dynamic_cast<ModularEcho*>(parseSrc("echo(\"a\") cube(1);")[0].get())->arguments.size(), 1u);
    EXPECT_EQ(dynamic_cast<ModularEcho*>(parseSrc("echo(\"a\", \"b\") cube(1);")[0].get())->arguments.size(), 2u);
    EXPECT_EQ(dynamic_cast<ModularEcho*>(parseSrc("echo(\"a\", \"b\", \"c\") cube(1);")[0].get())->arguments.size(), 3u);
}
TEST(EchoArguments, ModularAssertOneToThree) {
    EXPECT_EQ(dynamic_cast<ModularAssert*>(parseSrc("assert(x > 0) cube(x);")[0].get())->arguments.size(), 1u);
    EXPECT_EQ(dynamic_cast<ModularAssert*>(parseSrc("assert(x > 0, \"positive\") cube(x);")[0].get())->arguments.size(), 2u);
    EXPECT_EQ(
        dynamic_cast<ModularAssert*>(parseSrc("assert(x > 0, \"positive\", \"extra\") cube(x);")[0].get())->arguments.size(),
        3u);
}

TEST(ListComprehensionElements, ZeroToThree) {
    EXPECT_EQ(dynamic_cast<ListComprehension*>(E("[]"))->elements.size(), 0u);
    EXPECT_EQ(dynamic_cast<ListComprehension*>(E("[1]"))->elements.size(), 1u);
    EXPECT_EQ(dynamic_cast<ListComprehension*>(E("[1, 2]"))->elements.size(), 2u);
    EXPECT_EQ(dynamic_cast<ListComprehension*>(E("[1, 2, 3]"))->elements.size(), 3u);
}
TEST(ListComprehensionElements, ForElements) {
    auto* lc1 = dynamic_cast<ListComprehension*>(E("[for (i=[0:5]) i]"));
    ASSERT_EQ(lc1->elements.size(), 1u);
    EXPECT_NE(dynamic_cast<ListCompFor*>(lc1->elements[0].get()), nullptr);
    auto* lc2 = dynamic_cast<ListComprehension*>(E("[for (i=[0:5]) i, for (j=[0:5]) j]"));
    ASSERT_EQ(lc2->elements.size(), 2u);
    EXPECT_NE(dynamic_cast<ListCompFor*>(lc2->elements[0].get()), nullptr);
    EXPECT_NE(dynamic_cast<ListCompFor*>(lc2->elements[1].get()), nullptr);
}
TEST(ListComprehensionElements, IfElement) {
    auto* lc = dynamic_cast<ListComprehension*>(E("[for (i=[0:5]) if (i > 2) i]"));
    auto* forElem = dynamic_cast<ListCompFor*>(lc->elements[0].get());
    ASSERT_NE(forElem, nullptr);
    EXPECT_NE(dynamic_cast<ListCompIf*>(forElem->body.get()), nullptr);
}
TEST(ListComprehensionElements, IfElseElement) {
    auto* lc = dynamic_cast<ListComprehension*>(E("[for (i=[0:5]) if (i > 2) i else 0]"));
    auto* forElem = dynamic_cast<ListCompFor*>(lc->elements[0].get());
    ASSERT_NE(forElem, nullptr);
    EXPECT_NE(dynamic_cast<ListCompIfElse*>(forElem->body.get()), nullptr);
}
TEST(ListComprehensionElements, Str) {
    EXPECT_EQ(E("[1, 2, 3]")->toString(), "[1, 2, 3]");
    EXPECT_EQ(E("[]")->toString(), "[]");
}

TEST(LetAssignments, LetOpZeroToThree) {
    EXPECT_EQ(dynamic_cast<LetOp*>(E("let() x"))->assignments.size(), 0u);
    EXPECT_EQ(dynamic_cast<LetOp*>(E("let(a=1) a"))->assignments.size(), 1u);
    EXPECT_EQ(dynamic_cast<LetOp*>(E("let(a=1, b=2) a + b"))->assignments.size(), 2u);
    EXPECT_EQ(dynamic_cast<LetOp*>(E("let(a=1, b=2, c=3) a + b + c"))->assignments.size(), 3u);
}
