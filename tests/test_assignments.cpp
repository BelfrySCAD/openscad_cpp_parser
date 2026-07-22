// Ported from openscad_lalr_parser/tests/test_assignments.py.
#include "test_helpers.hpp"

#include <gtest/gtest.h>
#include <algorithm>

using namespace oscad;

// -- Assignments --------------------------------------------------------

TEST(Assignments, SimpleAssignment) {
    auto ast = parseSrc("x = 10;");
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->name->name, "x");
    auto* n = dynamic_cast<NumberLiteral*>(a->expr.get());
    ASSERT_NE(n, nullptr);
    EXPECT_DOUBLE_EQ(n->val, 10.0);
}

TEST(Assignments, ExpressionAssignment) {
    auto ast = parseSrc("y = x + 5;");
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    EXPECT_NE(dynamic_cast<AdditionOp*>(a->expr.get()), nullptr);
}

TEST(Assignments, MultipleAssignments) {
    auto ast = parseSrc("x = 1;\ny = 2;\nz = 3;");
    ASSERT_EQ(ast.size(), 3u);
    for (auto& n : ast) {
        EXPECT_NE(dynamic_cast<Assignment*>(n.get()), nullptr);
    }
}

TEST(Assignments, AssignmentStr) {
    auto ast = parseSrc("x = 10;");
    EXPECT_EQ(ast[0]->toString(), "x = 10");
}

TEST(Assignments, DollarVarAssignment) {
    auto ast = parseSrc("$fn = 64;");
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->name->name, "$fn");
}

TEST(Assignments, AssignmentPosition) {
    auto ast = parseSrc("x = 10;");
    EXPECT_EQ(ast[0]->position().line, 1);
}

TEST(Assignments, AssignmentWithExpression) {
    auto ast = parseSrc("x = 1 + 2;");
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    EXPECT_NE(dynamic_cast<AdditionOp*>(a->expr.get()), nullptr);
}

TEST(Assignments, AssignmentInBlock) {
    auto ast = parseSrc("module wrapper() { x = 1; y = 2; }");
    auto* decl = dynamic_cast<ModuleDeclaration*>(ast[0].get());
    ASSERT_NE(decl, nullptr);
    size_t count =
        std::count_if(decl->children.begin(), decl->children.end(),
                       [](const std::unique_ptr<ASTNode>& n) { return dynamic_cast<Assignment*>(n.get()) != nullptr; });
    EXPECT_EQ(count, 2u);
}

TEST(Assignments, AssignmentInModule) {
    auto ast = parseSrc("module test() { x = 1; }");
    auto* decl = dynamic_cast<ModuleDeclaration*>(ast[0].get());
    ASSERT_NE(decl, nullptr);
    ASSERT_EQ(decl->children.size(), 1u);
    auto* a = dynamic_cast<Assignment*>(decl->children[0].get());
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->name->name, "x");
}

// -- Statements -----------------------------------------------------------

TEST(Statements, EmptyStatement) {
    EXPECT_NO_THROW(parseSrc("cube(1);;"));
}

TEST(Statements, MultipleEmptyStatements) {
    EXPECT_NO_THROW(parseSrc("cube(1);;;"));
}

TEST(Statements, BlockStatement) {
    EXPECT_NO_THROW(parseSrc("module wrapper() {}"));
}

TEST(Statements, BlockWithStatements) {
    auto ast = parseSrc("module wrapper() { x = 1; y = 2; }");
    EXPECT_NE(dynamic_cast<ModuleDeclaration*>(ast[0].get()), nullptr);
}

TEST(Statements, NestedBlocks) {
    auto ast = parseSrc("module outer() { module inner() { x = 1; } }");
    EXPECT_NE(dynamic_cast<ModuleDeclaration*>(ast[0].get()), nullptr);
}

// -- Argument lists -----------------------------------------------------

TEST(ArgumentLists, EmptyArguments) {
    auto ast = parseSrc("test();");
    auto* c = dynamic_cast<ModularCall*>(ast[0].get());
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->arguments.size(), 0u);
}

TEST(ArgumentLists, ArgumentsTrailingComma) {
    auto ast = parseSrc("test(1, 2,);");
    EXPECT_NE(dynamic_cast<ModularCall*>(ast[0].get()), nullptr);
}

TEST(ArgumentLists, ArgumentsNamed) {
    auto ast = parseSrc("test(x=1, y=2);");
    auto* c = dynamic_cast<ModularCall*>(ast[0].get());
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->arguments.size(), 2u);
}

TEST(ArgumentLists, ArgumentsMixed) {
    auto ast = parseSrc("test(1, y=2);");
    auto* c = dynamic_cast<ModularCall*>(ast[0].get());
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->arguments.size(), 2u);
}

// -- Assignment expressions (for/let clauses) ----------------------------

TEST(AssignmentExpressions, Simple) {
    auto ast = parseSrc("for (i = [0:1]) cube(1);");
    EXPECT_NE(dynamic_cast<ModularFor*>(ast[0].get()), nullptr);
}

TEST(AssignmentExpressions, Multiple) {
    auto ast = parseSrc("for (i = [0:1], j = [0:1]) cube(1);");
    EXPECT_NE(dynamic_cast<ModularFor*>(ast[0].get()), nullptr);
}

TEST(AssignmentExpressions, InLet) {
    auto ast = parseSrc("let(x = 1, y = 2) cube(1);");
    EXPECT_NE(dynamic_cast<ModularLet*>(ast[0].get()), nullptr);
}

// -- Scope analysis (mirrors TestScopeAnalysis) --------------------------

TEST(AssignmentsScopeAnalysis, BuildScopes) {
    auto ast = parseSrc("x = 10;\ny = 20;");
    auto scope = buildScopes(ast);
    EXPECT_NE(scope->lookupVariable("x"), nullptr);
    EXPECT_NE(scope->lookupVariable("y"), nullptr);
    EXPECT_EQ(scope->lookupVariable("z"), nullptr);
}

TEST(AssignmentsScopeAnalysis, FunctionScope) {
    auto ast = parseSrc("function add(a, b) = a + b;");
    auto scope = buildScopes(ast);
    EXPECT_NE(scope->lookupFunction("add"), nullptr);
    EXPECT_EQ(scope->lookupVariable("add"), nullptr);
}

TEST(AssignmentsScopeAnalysis, ModuleScope) {
    auto ast = parseSrc("module box(size) { cube(size); }");
    auto scope = buildScopes(ast);
    EXPECT_NE(scope->lookupModule("box"), nullptr);
}
