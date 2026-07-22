#include "openscad_cpp_parser/api.hpp"

#include <gtest/gtest.h>

using namespace oscad;

TEST(Smoke, ParsesSimpleAssignment) {
    auto ast = parseAst("x = 42;");
    ASSERT_EQ(ast.size(), 1u);
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->name->name, "x");
    auto* num = dynamic_cast<NumberLiteral*>(a->expr.get());
    ASSERT_NE(num, nullptr);
    EXPECT_DOUBLE_EQ(num->val, 42.0);
}

TEST(Smoke, OperatorPrecedence) {
    // 1 + 2 * 3 should parse as 1 + (2 * 3), not (1 + 2) * 3.
    auto ast = parseAst("x = 1 + 2 * 3;");
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    auto* add = dynamic_cast<AdditionOp*>(a->expr.get());
    ASSERT_NE(add, nullptr);
    auto* left = dynamic_cast<NumberLiteral*>(add->left.get());
    ASSERT_NE(left, nullptr);
    EXPECT_DOUBLE_EQ(left->val, 1.0);
    auto* right = dynamic_cast<MultiplicationOp*>(add->right.get());
    ASSERT_NE(right, nullptr);
}

TEST(Smoke, ExponentBindsTighterThanUnaryMinus) {
    // -2^2 == -(2^2), matching Python's operator precedence.
    auto ast = parseAst("x = -2^2;");
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    auto* neg = dynamic_cast<UnaryMinusOp*>(a->expr.get());
    ASSERT_NE(neg, nullptr);
    auto* exp = dynamic_cast<ExponentOp*>(neg->expr.get());
    ASSERT_NE(exp, nullptr);
}

TEST(Smoke, ModuleCallWithBlockChildrenKeepsAllStatements) {
    // Highest-risk finding from the plan: a `{ ... }` body must not be
    // truncated to a single statement.
    auto ast = parseAst("for (i = [0:2]) { cube(1); sphere(1); }");
    ASSERT_EQ(ast.size(), 1u);
    auto* forNode = dynamic_cast<ModularFor*>(ast[0].get());
    ASSERT_NE(forNode, nullptr);
    ASSERT_EQ(forNode->body.size(), 2u);
}

TEST(Smoke, DanglingElseAttachesToInnerIf) {
    auto ast = parseAst("if (true) if (false) cube(1); else sphere(1);");
    ASSERT_EQ(ast.size(), 1u);
    auto* outerIf = dynamic_cast<ModularIf*>(ast[0].get());
    ASSERT_NE(outerIf, nullptr);
    ASSERT_EQ(outerIf->trueBranch.size(), 1u);
    auto* innerIfElse = dynamic_cast<ModularIfElse*>(outerIf->trueBranch[0].get());
    ASSERT_NE(innerIfElse, nullptr);
}

TEST(Smoke, ParameterDefaultResolvesInCallerScope) {
    auto ast = parseAst("y = 10;\nfunction f(a = y) = a;");
    ASSERT_EQ(ast.size(), 2u);
    auto root = buildScopes(ast);
    auto* func = dynamic_cast<FunctionDeclaration*>(ast[1].get());
    ASSERT_NE(func, nullptr);
    ASSERT_EQ(func->parameters.size(), 1u);
    auto* defaultExpr = func->parameters[0]->defaultValue.get();
    ASSERT_NE(defaultExpr, nullptr);
    ASSERT_NE(defaultExpr->scope(), nullptr);
    EXPECT_NE(defaultExpr->scope()->lookupVariable("y"), nullptr);
}

TEST(Smoke, LetOpSequentialSelfReferentialBinding) {
    auto ast = parseAst("z = let(x = 1, y = x + 1) y;");
    auto root = buildScopes(ast);
    auto* assign = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(assign, nullptr);
    auto* let = dynamic_cast<LetOp*>(assign->expr.get());
    ASSERT_NE(let, nullptr);
    ASSERT_EQ(let->assignments.size(), 2u);
    // y's RHS (x + 1) should resolve `x` to the first let-assignment.
    auto* yRhs = dynamic_cast<AdditionOp*>(let->assignments[1]->expr.get());
    ASSERT_NE(yRhs, nullptr);
    ASSERT_NE(yRhs->scope(), nullptr);
    EXPECT_EQ(yRhs->scope()->lookupVariable("x"), let->assignments[0].get());
}

TEST(Smoke, ModularIfElseBranchesAreIndependentScopes) {
    auto ast = parseAst("if (true) { a = 1; } else { a = 2; }");
    auto root = buildScopes(ast);
    auto* ifElse = dynamic_cast<ModularIfElse*>(ast[0].get());
    ASSERT_NE(ifElse, nullptr);
    Scope* trueScope = ifElse->trueBranch[0]->scope();
    Scope* falseScope = ifElse->falseBranch[0]->scope();
    ASSERT_NE(trueScope, nullptr);
    ASSERT_NE(falseScope, nullptr);
    EXPECT_NE(trueScope, falseScope);
    EXPECT_EQ(trueScope->parent(), falseScope->parent());
}

TEST(Smoke, HeterogeneousVectorElements) {
    auto ast = parseAst("x = [1, for (i = [1, 2]) i, 3];");
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    auto* vec = dynamic_cast<ListComprehension*>(a->expr.get());
    ASSERT_NE(vec, nullptr);
    ASSERT_EQ(vec->elements.size(), 3u);
    EXPECT_EQ(vec->elements[0]->kind(), NodeKind::NumberLiteral);
    EXPECT_EQ(vec->elements[1]->kind(), NodeKind::ListCompFor);
    EXPECT_EQ(vec->elements[2]->kind(), NodeKind::NumberLiteral);
}
