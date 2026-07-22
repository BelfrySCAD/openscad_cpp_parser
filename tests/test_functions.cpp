// Ported from openscad_lalr_parser/tests/test_functions.py.
#include "test_helpers.hpp"

#include <gtest/gtest.h>

using namespace oscad;

// -- Function declarations --------------------------------------------

TEST(FunctionDeclarations, SimpleFunction) {
    auto ast = parseSrc("function add(a, b) = a + b;");
    auto* f = dynamic_cast<FunctionDeclaration*>(ast[0].get());
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->name->name, "add");
    EXPECT_EQ(f->parameters.size(), 2u);
    EXPECT_NE(dynamic_cast<AdditionOp*>(f->expr.get()), nullptr);
}

TEST(FunctionDeclarations, FunctionWithDefaults) {
    auto ast = parseSrc("function foo(x, y=10) = x + y;");
    auto* f = dynamic_cast<FunctionDeclaration*>(ast[0].get());
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->parameters[0]->defaultValue, nullptr);
    EXPECT_NE(f->parameters[1]->defaultValue, nullptr);
}

TEST(FunctionDeclarations, FunctionNoParams) {
    auto ast = parseSrc("function pi() = 3.14159;");
    auto* f = dynamic_cast<FunctionDeclaration*>(ast[0].get());
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->parameters.size(), 0u);
}

TEST(FunctionDeclarations, FunctionStr) {
    auto ast = parseSrc("function add(a, b) = a + b;");
    std::string s = ast[0]->toString();
    EXPECT_NE(s.find("function"), std::string::npos);
    EXPECT_NE(s.find("add"), std::string::npos);
}

TEST(FunctionDeclarations, FunctionComplexExpression) {
    auto ast = parseSrc("function complex(x) = x * 2 + sin(x) * cos(x);");
    auto* f = dynamic_cast<FunctionDeclaration*>(ast[0].get());
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->name->name, "complex");
}

TEST(FunctionDeclarations, FunctionTernary) {
    auto ast = parseSrc("function abs(x) = x >= 0 ? x : -x;");
    auto* f = dynamic_cast<FunctionDeclaration*>(ast[0].get());
    ASSERT_NE(f, nullptr);
    EXPECT_NE(dynamic_cast<TernaryOp*>(f->expr.get()), nullptr);
}

TEST(FunctionDeclarations, FunctionWithLet) {
    auto ast = parseSrc("function test(x) = let(y = x * 2) y + 1;");
    auto* f = dynamic_cast<FunctionDeclaration*>(ast[0].get());
    ASSERT_NE(f, nullptr);
    EXPECT_NE(dynamic_cast<LetOp*>(f->expr.get()), nullptr);
}

// -- Function literals --------------------------------------------------

TEST(FunctionLiteral, Basic) {
    auto ast = parseSrc("x = function(a) a * 2;");
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    auto* fl = dynamic_cast<FunctionLiteral*>(a->expr.get());
    ASSERT_NE(fl, nullptr);
    EXPECT_EQ(fl->parameters.size(), 1u);
    EXPECT_NE(dynamic_cast<MultiplicationOp*>(fl->body.get()), nullptr);
}

TEST(FunctionLiteral, NoParams) {
    auto ast = parseSrc("x = function() 42;");
    auto* fl = dynamic_cast<FunctionLiteral*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(fl, nullptr);
    EXPECT_EQ(fl->parameters.size(), 0u);
}

TEST(FunctionLiteral, TwoParams) {
    auto ast = parseSrc("x = function(a, b) a + b;");
    auto* fl = dynamic_cast<FunctionLiteral*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(fl, nullptr);
    EXPECT_EQ(fl->parameters.size(), 2u);
}

TEST(FunctionLiteral, ThreeParams) {
    auto ast = parseSrc("x = function(a, b, c) a + b + c;");
    auto* fl = dynamic_cast<FunctionLiteral*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(fl, nullptr);
    EXPECT_EQ(fl->parameters.size(), 3u);
}

TEST(FunctionLiteral, ParamsWithDefaults) {
    auto ast = parseSrc("x = function(a, b=2, c=3) a + b + c;");
    auto* fl = dynamic_cast<FunctionLiteral*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(fl, nullptr);
    ASSERT_EQ(fl->parameters.size(), 3u);
    EXPECT_EQ(fl->parameters[0]->defaultValue, nullptr);
    EXPECT_NE(fl->parameters[1]->defaultValue, nullptr);
    EXPECT_NE(fl->parameters[2]->defaultValue, nullptr);
}

TEST(FunctionLiteral, Str) {
    auto ast = parseSrc("x = function(a) a;");
    auto* fl = dynamic_cast<Assignment*>(ast[0].get())->expr.get();
    EXPECT_NE(fl->toString().find("function"), std::string::npos);
}

TEST(FunctionLiteral, CallOnLiteral) {
    auto ast = parseSrc("x = (function(a) a * 2)(5);");
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    EXPECT_NE(dynamic_cast<PrimaryCall*>(a->expr.get()), nullptr);
}

// -- Function calls -----------------------------------------------------

TEST(FunctionCall, NoArgs) {
    auto ast = parseSrc("x = test();");
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    EXPECT_NE(dynamic_cast<PrimaryCall*>(a->expr.get()), nullptr);
}

TEST(FunctionCall, SingleArg) {
    auto ast = parseSrc("x = test(5);");
    EXPECT_NE(dynamic_cast<PrimaryCall*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get()), nullptr);
}

TEST(FunctionCall, MultipleArgs) {
    auto ast = parseSrc("x = add(1, 2);");
    EXPECT_NE(dynamic_cast<PrimaryCall*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get()), nullptr);
}

TEST(FunctionCall, NamedArgs) {
    auto ast = parseSrc("x = test(x=1, y=2);");
    EXPECT_NE(dynamic_cast<PrimaryCall*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get()), nullptr);
}

TEST(FunctionCall, MixedArgs) {
    auto ast = parseSrc("x = test(1, y=2);");
    EXPECT_NE(dynamic_cast<PrimaryCall*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get()), nullptr);
}

TEST(FunctionCall, Nested) {
    auto ast = parseSrc("x = add(multiply(2, 3), 4);");
    EXPECT_NE(dynamic_cast<PrimaryCall*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get()), nullptr);
}

TEST(FunctionCall, InExpression) {
    auto ast = parseSrc("x = add(1, 2) * 3;");
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    EXPECT_NE(dynamic_cast<MultiplicationOp*>(a->expr.get()), nullptr);
}

// -- Function declaration details ----------------------------------------

TEST(FunctionDeclarationDetailed, OneParamNoDefault) {
    auto ast = parseSrc("function foo(x) = x;");
    auto* f = dynamic_cast<FunctionDeclaration*>(ast[0].get());
    ASSERT_NE(f, nullptr);
    ASSERT_EQ(f->parameters.size(), 1u);
    EXPECT_EQ(f->parameters[0]->name->name, "x");
    EXPECT_EQ(f->parameters[0]->defaultValue, nullptr);
    EXPECT_NE(dynamic_cast<Identifier*>(f->expr.get()), nullptr);
}

TEST(FunctionDeclarationDetailed, OneParamWithDefault) {
    auto ast = parseSrc("function foo(x=10) = x;");
    auto* f = dynamic_cast<FunctionDeclaration*>(ast[0].get());
    ASSERT_NE(f, nullptr);
    auto* n = dynamic_cast<NumberLiteral*>(f->parameters[0]->defaultValue.get());
    ASSERT_NE(n, nullptr);
    EXPECT_DOUBLE_EQ(n->val, 10.0);
}

TEST(FunctionDeclarationDetailed, TwoParamsNoDefaults) {
    auto ast = parseSrc("function foo(x, y) = x + y;");
    auto* f = dynamic_cast<FunctionDeclaration*>(ast[0].get());
    ASSERT_NE(f, nullptr);
    ASSERT_EQ(f->parameters.size(), 2u);
    EXPECT_EQ(f->parameters[0]->defaultValue, nullptr);
    EXPECT_EQ(f->parameters[1]->defaultValue, nullptr);
}

TEST(FunctionDeclarationDetailed, TwoParamsWithDefaults) {
    auto ast = parseSrc("function foo(x=1, y=2) = x + y;");
    auto* f = dynamic_cast<FunctionDeclaration*>(ast[0].get());
    ASSERT_NE(f, nullptr);
    EXPECT_DOUBLE_EQ(dynamic_cast<NumberLiteral*>(f->parameters[0]->defaultValue.get())->val, 1.0);
    EXPECT_DOUBLE_EQ(dynamic_cast<NumberLiteral*>(f->parameters[1]->defaultValue.get())->val, 2.0);
}

TEST(FunctionDeclarationDetailed, MixedParams) {
    auto ast = parseSrc("function foo(x, y=2, z) = x + y + z;");
    auto* f = dynamic_cast<FunctionDeclaration*>(ast[0].get());
    ASSERT_NE(f, nullptr);
    ASSERT_EQ(f->parameters.size(), 3u);
    EXPECT_EQ(f->parameters[0]->defaultValue, nullptr);
    EXPECT_NE(f->parameters[1]->defaultValue, nullptr);
    EXPECT_EQ(f->parameters[2]->defaultValue, nullptr);
}
