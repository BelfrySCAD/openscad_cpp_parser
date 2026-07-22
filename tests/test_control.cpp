// Ported from openscad_lalr_parser/tests/test_control.py.
#include "test_helpers.hpp"

#include <gtest/gtest.h>

using namespace oscad;

// -- ModularIf / ModularIfElse -------------------------------------------

TEST(ModularIfTest, IfStatement) {
    EXPECT_NE(dynamic_cast<ModularIf*>(parseSrc("if (x > 0) cube(x);")[0].get()), nullptr);
}
TEST(ModularIfTest, IfWithBlock) {
    EXPECT_NE(dynamic_cast<ModularIf*>(parseSrc("if (x > 0) { cube(x); sphere(1); }")[0].get()), nullptr);
}
TEST(ModularIfTest, IfElseStatement) {
    EXPECT_NE(dynamic_cast<ModularIfElse*>(parseSrc("if (x > 0) cube(x); else sphere(5);")[0].get()), nullptr);
}
TEST(ModularIfTest, IfElseWithBlocks) {
    EXPECT_NE(dynamic_cast<ModularIfElse*>(parseSrc("if (x > 0) { cube(x); } else { sphere(5); }")[0].get()), nullptr);
}
TEST(ModularIfTest, IfNested) {
    // Dangling else binds to the inner if -- outer statement is a plain ModularIf.
    EXPECT_NE(dynamic_cast<ModularIf*>(parseSrc("if (true) if (false) cube(10); else sphere(5);")[0].get()), nullptr);
}
TEST(ModularIfTest, IfWithExpression) {
    EXPECT_NE(dynamic_cast<ModularIf*>(parseSrc("if (x > 0 && y < 10) cube(10);")[0].get()), nullptr);
}

// -- ModularFor -----------------------------------------------------------

TEST(ModularForTest, Simple) {
    auto ast = parseSrc("for (i=[0:5]) cube(i);");
    auto* f = dynamic_cast<ModularFor*>(ast[0].get());
    ASSERT_NE(f, nullptr);
    ASSERT_EQ(f->assignments.size(), 1u);
    EXPECT_EQ(f->assignments[0]->name->name, "i");
}
TEST(ModularForTest, MultiVariable) {
    auto ast = parseSrc("for (i=[0:2], j=[0:2]) translate([i,j,0]) cube(1);");
    auto* f = dynamic_cast<ModularFor*>(ast[0].get());
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->assignments.size(), 2u);
}
TEST(ModularForTest, RangeWithStep) {
    EXPECT_NE(dynamic_cast<ModularFor*>(parseSrc("for (i=[0:2:10]) translate([i, 0, 0]) cube(1);")[0].get()), nullptr);
}
TEST(ModularForTest, ForVector) {
    EXPECT_NE(dynamic_cast<ModularFor*>(parseSrc("for (i=[1, 2, 3]) translate([i, 0, 0]) cube(1);")[0].get()), nullptr);
}
TEST(ModularForTest, WithBlock) {
    EXPECT_NE(dynamic_cast<ModularFor*>(parseSrc("for (i=[0:5]) { translate([i,0,0]) cube(1); }")[0].get()), nullptr);
}

// -- ModularIntersectionFor -----------------------------------------------

TEST(ModularIntersectionForTest, Basic) {
    auto ast = parseSrc("intersection_for (i=[0:3]) rotate([0,0,i*90]) cube(10);");
    auto* f = dynamic_cast<ModularIntersectionFor*>(ast[0].get());
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->assignments.size(), 1u);
}
TEST(ModularIntersectionForTest, WithBlock) {
    EXPECT_NE(dynamic_cast<ModularIntersectionFor*>(parseSrc("intersection_for(i=[0:5]) { translate([i, 0, 0]) cube(1); }")[0].get()),
              nullptr);
}
TEST(ModularIntersectionForTest, MultipleVars) {
    auto ast = parseSrc("intersection_for(i=[0:5], j=[0:3]) translate([i, j, 0]) cube(1);");
    auto* f = dynamic_cast<ModularIntersectionFor*>(ast[0].get());
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->assignments.size(), 2u);
}

// -- ModularLet -------------------------------------------------------

TEST(ModularLetTest, Basic) {
    auto ast = parseSrc("let (x=10) cube(x);");
    auto* l = dynamic_cast<ModularLet*>(ast[0].get());
    ASSERT_NE(l, nullptr);
    ASSERT_EQ(l->assignments.size(), 1u);
    EXPECT_EQ(l->assignments[0]->name->name, "x");
}
TEST(ModularLetTest, Multiple) {
    auto ast = parseSrc("let(x=10, y=20) cube([x, y, 10]);");
    auto* l = dynamic_cast<ModularLet*>(ast[0].get());
    ASSERT_NE(l, nullptr);
    EXPECT_EQ(l->assignments.size(), 2u);
}
TEST(ModularLetTest, WithBlock) {
    EXPECT_NE(dynamic_cast<ModularLet*>(parseSrc("let(x=10) { cube(x); }")[0].get()), nullptr);
}
TEST(ModularLetTest, Nested) {
    auto ast = parseSrc("let(x=10) let(y=x*2) cube(y);");
    auto* l = dynamic_cast<ModularLet*>(ast[0].get());
    ASSERT_NE(l, nullptr);
    ASSERT_EQ(l->children.size(), 1u);
    EXPECT_NE(dynamic_cast<ModularLet*>(l->children[0].get()), nullptr);
}

// -- ModularEcho ------------------------------------------------------

TEST(ModularEchoTest, Basic) {
    auto ast = parseSrc("echo(\"hello\") cube(1);");
    auto* e = dynamic_cast<ModularEcho*>(ast[0].get());
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->arguments.size(), 1u);
}
TEST(ModularEchoTest, MultipleArgs) {
    auto ast = parseSrc("echo(\"Hello\", \"World\") cube(10);");
    auto* e = dynamic_cast<ModularEcho*>(ast[0].get());
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->arguments.size(), 2u);
}
TEST(ModularEchoTest, WithBlock) {
    EXPECT_NE(dynamic_cast<ModularEcho*>(parseSrc("echo(\"Hello\") { cube(10); }")[0].get()), nullptr);
}

// -- ModularAssert ------------------------------------------------------

TEST(ModularAssertTest, Basic) {
    auto ast = parseSrc("assert(x > 0) cube(x);");
    auto* a = dynamic_cast<ModularAssert*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->arguments.size(), 1u);
}
TEST(ModularAssertTest, WithMessage) {
    auto ast = parseSrc("assert(true, \"Error message\") cube(10);");
    auto* a = dynamic_cast<ModularAssert*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->arguments.size(), 2u);
}
TEST(ModularAssertTest, WithBlock) {
    EXPECT_NE(dynamic_cast<ModularAssert*>(parseSrc("assert(true) { cube(10); }")[0].get()), nullptr);
}

// -- LetOp (expression-position let) -------------------------------------

TEST(LetExprTest, Basic) {
    auto ast = parseSrc("x = let(a=1, b=2) a + b;");
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    auto* let = dynamic_cast<LetOp*>(a->expr.get());
    ASSERT_NE(let, nullptr);
    EXPECT_EQ(let->assignments.size(), 2u);
}
TEST(LetExprTest, Nested) {
    auto ast = parseSrc("x = let(a=1) let(b=2) a + b;");
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    auto* let = dynamic_cast<LetOp*>(a->expr.get());
    ASSERT_NE(let, nullptr);
    EXPECT_NE(dynamic_cast<LetOp*>(let->body.get()), nullptr);
}

// -- EchoOp / AssertOp (expression-position) ------------------------------

TEST(EchoExprTest, Basic) {
    auto ast = parseSrc("x = echo(\"val\", y) y;");
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    auto* echo = dynamic_cast<EchoOp*>(a->expr.get());
    ASSERT_NE(echo, nullptr);
    EXPECT_EQ(echo->arguments.size(), 2u);
}
TEST(EchoExprTest, NoBodyDefaultsToUndefined) {
    auto ast = parseSrc("x = echo(\"val\");");
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    auto* echo = dynamic_cast<EchoOp*>(a->expr.get());
    ASSERT_NE(echo, nullptr);
    EXPECT_NE(dynamic_cast<UndefinedLiteral*>(echo->body.get()), nullptr);
}

TEST(AssertExprTest, Basic) {
    auto ast = parseSrc("x = assert(y > 0) y;");
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    EXPECT_NE(dynamic_cast<AssertOp*>(a->expr.get()), nullptr);
}
TEST(AssertExprTest, NoBodyDefaultsToUndefined) {
    auto ast = parseSrc("x = assert(y > 0);");
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    auto* assertOp = dynamic_cast<AssertOp*>(a->expr.get());
    ASSERT_NE(assertOp, nullptr);
    EXPECT_NE(dynamic_cast<UndefinedLiteral*>(assertOp->body.get()), nullptr);
}

// -- Argument/assignment count sweeps -------------------------------------

TEST(ModularEchoArgCounts, Zero) {
    auto ast = parseSrc("echo();");
    auto* e = dynamic_cast<ModularEcho*>(ast[0].get());
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->arguments.size(), 0u);
}
TEST(ModularEchoArgCounts, Two) {
    auto ast = parseSrc("echo(1, 2);");
    auto* e = dynamic_cast<ModularEcho*>(ast[0].get());
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->arguments.size(), 2u);
}
TEST(ModularEchoArgCounts, Three) {
    auto ast = parseSrc("echo(1, 2, 3);");
    auto* e = dynamic_cast<ModularEcho*>(ast[0].get());
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->arguments.size(), 3u);
}

TEST(ModularAssertArgCounts, Two) {
    auto ast = parseSrc("assert(true, \"msg\");");
    auto* a = dynamic_cast<ModularAssert*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->arguments.size(), 2u);
}

TEST(ModularLetAssignmentCounts, Zero) {
    auto ast = parseSrc("let() cube(1);");
    auto* l = dynamic_cast<ModularLet*>(ast[0].get());
    ASSERT_NE(l, nullptr);
    EXPECT_EQ(l->assignments.size(), 0u);
}
TEST(ModularLetAssignmentCounts, Two) {
    auto ast = parseSrc("let(x=1, y=2) cube(x);");
    auto* l = dynamic_cast<ModularLet*>(ast[0].get());
    ASSERT_NE(l, nullptr);
    EXPECT_EQ(l->assignments.size(), 2u);
}
TEST(ModularLetAssignmentCounts, Three) {
    auto ast = parseSrc("let(x=1, y=2, z=3) cube(x);");
    auto* l = dynamic_cast<ModularLet*>(ast[0].get());
    ASSERT_NE(l, nullptr);
    EXPECT_EQ(l->assignments.size(), 3u);
}

TEST(ModularForAssignmentCounts, Two) {
    auto ast = parseSrc("for (i=[0:3], j=[0:2]) cube(i);");
    auto* f = dynamic_cast<ModularFor*>(ast[0].get());
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->assignments.size(), 2u);
}
TEST(ModularForAssignmentCounts, Three) {
    auto ast = parseSrc("for (i=[0:3], j=[0:2], k=[0:1]) cube(i);");
    auto* f = dynamic_cast<ModularFor*>(ast[0].get());
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->assignments.size(), 3u);
}

TEST(ModularIntersectionForCounts, Two) {
    auto ast = parseSrc("intersection_for (i=[0:3], j=[0:2]) cube(i);");
    auto* f = dynamic_cast<ModularIntersectionFor*>(ast[0].get());
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->assignments.size(), 2u);
}

// -- each (shallow smoke tests, matching the reference) -------------------

TEST(EachTest, InListcomp) {
    auto ast = parseSrc("x = [each [1, 2, 3]];");
    EXPECT_NE(dynamic_cast<Assignment*>(ast[0].get()), nullptr);
}
TEST(EachTest, Nested) {
    auto ast = parseSrc("x = [each [each [1, 2, 3]]];");
    EXPECT_NE(dynamic_cast<Assignment*>(ast[0].get()), nullptr);
}
