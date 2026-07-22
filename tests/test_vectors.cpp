// Ported from openscad_lalr_parser/tests/test_vectors.py.
#include "test_helpers.hpp"

#include <gtest/gtest.h>
#include <algorithm>

using namespace oscad;

namespace {
std::vector<std::unique_ptr<ASTNode>> gAst;
Expression* E(const std::string& code) {
    return exprSrc(code, gAst);
}
} // namespace

// -- Vectors --------------------------------------------------------------

TEST(Vectors, EmptyVector) {
    auto* lc = dynamic_cast<ListComprehension*>(E("[]"));
    ASSERT_NE(lc, nullptr);
    EXPECT_EQ(lc->elements.size(), 0u);
}
TEST(Vectors, SimpleVector) {
    auto* lc = dynamic_cast<ListComprehension*>(E("[1, 2, 3]"));
    ASSERT_NE(lc, nullptr);
    EXPECT_EQ(lc->elements.size(), 3u);
}
TEST(Vectors, NestedVector) {
    auto* lc = dynamic_cast<ListComprehension*>(E("[[1, 2], [3, 4]]"));
    ASSERT_NE(lc, nullptr);
    ASSERT_EQ(lc->elements.size(), 2u);
    EXPECT_NE(dynamic_cast<ListComprehension*>(lc->elements[0].get()), nullptr);
}
TEST(Vectors, TrailingComma) {
    auto* lc = dynamic_cast<ListComprehension*>(E("[1, 2, 3,]"));
    ASSERT_NE(lc, nullptr);
    EXPECT_EQ(lc->elements.size(), 3u);
}
TEST(Vectors, VectorStr) {
    EXPECT_EQ(E("[1, 2, 3]")->toString(), "[1, 2, 3]");
}
TEST(Vectors, SingleElement) {
    auto* lc = dynamic_cast<ListComprehension*>(E("[1]"));
    ASSERT_NE(lc, nullptr);
    EXPECT_EQ(lc->elements.size(), 1u);
}
TEST(Vectors, MixedTypes) {
    auto* lc = dynamic_cast<ListComprehension*>(E("[1, \"hello\", true]"));
    ASSERT_NE(lc, nullptr);
    ASSERT_EQ(lc->elements.size(), 3u);
    EXPECT_NE(dynamic_cast<NumberLiteral*>(lc->elements[0].get()), nullptr);
    EXPECT_NE(dynamic_cast<StringLiteral*>(lc->elements[1].get()), nullptr);
    EXPECT_NE(dynamic_cast<BooleanLiteral*>(lc->elements[2].get()), nullptr);
}
TEST(Vectors, WithExpressions) {
    auto* lc = dynamic_cast<ListComprehension*>(E("[1 + 2, 3 * 4, 5 / 6]"));
    ASSERT_NE(lc, nullptr);
    EXPECT_EQ(lc->elements.size(), 3u);
}

// -- Ranges -----------------------------------------------------------

TEST(Ranges, TwoPart) {
    auto* r = dynamic_cast<RangeLiteral*>(E("[0:10]"));
    ASSERT_NE(r, nullptr);
    EXPECT_DOUBLE_EQ(dynamic_cast<NumberLiteral*>(r->start.get())->val, 0.0);
    EXPECT_DOUBLE_EQ(dynamic_cast<NumberLiteral*>(r->end.get())->val, 10.0);
    EXPECT_DOUBLE_EQ(dynamic_cast<NumberLiteral*>(r->step.get())->val, 1.0);
}
TEST(Ranges, ThreePart) {
    auto* r = dynamic_cast<RangeLiteral*>(E("[0:2:10]"));
    ASSERT_NE(r, nullptr);
    EXPECT_DOUBLE_EQ(dynamic_cast<NumberLiteral*>(r->start.get())->val, 0.0);
    EXPECT_DOUBLE_EQ(dynamic_cast<NumberLiteral*>(r->step.get())->val, 2.0);
    EXPECT_DOUBLE_EQ(dynamic_cast<NumberLiteral*>(r->end.get())->val, 10.0);
}
TEST(Ranges, Str) {
    std::string s = E("[0:2:10]")->toString();
    EXPECT_NE(s.find('0'), std::string::npos);
    EXPECT_NE(s.find("10"), std::string::npos);
}
TEST(Ranges, Negative) {
    EXPECT_NE(dynamic_cast<RangeLiteral*>(E("[-5:5]")), nullptr);
}
TEST(Ranges, Expressions) {
    EXPECT_NE(dynamic_cast<RangeLiteral*>(E("[0:2*5:10]")), nullptr);
}

// -- List comprehensions --------------------------------------------------

TEST(ListComprehensions, ForComprehension) {
    auto* lc = dynamic_cast<ListComprehension*>(E("[for (i=[0:5]) i]"));
    ASSERT_NE(lc, nullptr);
    ASSERT_EQ(lc->elements.size(), 1u);
    EXPECT_NE(dynamic_cast<ListCompFor*>(lc->elements[0].get()), nullptr);
}
TEST(ListComprehensions, CForComprehension) {
    auto* lc = dynamic_cast<ListComprehension*>(E("[for (i=0; i<5; i=i+1) i]"));
    ASSERT_NE(lc, nullptr);
    EXPECT_NE(dynamic_cast<ListCompCFor*>(lc->elements[0].get()), nullptr);
}
TEST(ListComprehensions, IfComprehension) {
    auto* lc = dynamic_cast<ListComprehension*>(E("[for (i=[0:5]) if (i > 2) i]"));
    ASSERT_NE(lc, nullptr);
    auto* forElem = dynamic_cast<ListCompFor*>(lc->elements[0].get());
    ASSERT_NE(forElem, nullptr);
    EXPECT_NE(dynamic_cast<ListCompIf*>(forElem->body.get()), nullptr);
}
TEST(ListComprehensions, IfElseComprehension) {
    auto* lc = dynamic_cast<ListComprehension*>(E("[for (i=[0:5]) if (i > 2) i else -i]"));
    ASSERT_NE(lc, nullptr);
    auto* forElem = dynamic_cast<ListCompFor*>(lc->elements[0].get());
    ASSERT_NE(forElem, nullptr);
    EXPECT_NE(dynamic_cast<ListCompIfElse*>(forElem->body.get()), nullptr);
}
TEST(ListComprehensions, LetComprehension) {
    auto* lc = dynamic_cast<ListComprehension*>(E("[let (a=1) for (i=[0:3]) i]"));
    ASSERT_NE(lc, nullptr);
    EXPECT_NE(dynamic_cast<ListCompLet*>(lc->elements[0].get()), nullptr);
}
TEST(ListComprehensions, LetExprInVectorIsLetOp) {
    // A bare `let(...)` (no following `for`) is a LetOp, NOT ListCompLet.
    auto* lc = dynamic_cast<ListComprehension*>(E("[let (a=1) a]"));
    ASSERT_NE(lc, nullptr);
    EXPECT_NE(dynamic_cast<LetOp*>(lc->elements[0].get()), nullptr);
}
TEST(ListComprehensions, EachComprehension) {
    auto* lc = dynamic_cast<ListComprehension*>(E("[each [1, 2, 3]]"));
    ASSERT_NE(lc, nullptr);
    EXPECT_NE(dynamic_cast<ListCompEach*>(lc->elements[0].get()), nullptr);
}
TEST(ListComprehensions, MultipleAssignmentsFor) {
    auto* lc = dynamic_cast<ListComprehension*>(E("[for (i=[0:2], j=[0:2]) [i, j]]"));
    ASSERT_NE(lc, nullptr);
    auto* forElem = dynamic_cast<ListCompFor*>(lc->elements[0].get());
    ASSERT_NE(forElem, nullptr);
    EXPECT_EQ(forElem->assignments.size(), 2u);
}
TEST(ListComprehensions, EachAstDetail) {
    auto* outer = dynamic_cast<ListComprehension*>(E("[each [1, 2, 3]]"));
    ASSERT_NE(outer, nullptr);
    ASSERT_EQ(outer->elements.size(), 1u);
    auto* each = dynamic_cast<ListCompEach*>(outer->elements[0].get());
    ASSERT_NE(each, nullptr);
    auto* inner = dynamic_cast<ListComprehension*>(each->body.get());
    ASSERT_NE(inner, nullptr);
    ASSERT_EQ(inner->elements.size(), 3u);
    for (auto& e : inner->elements) {
        EXPECT_NE(dynamic_cast<NumberLiteral*>(e.get()), nullptr);
    }
}
TEST(ListComprehensions, EachStr) {
    EXPECT_EQ(E("[each [1, 2, 3]]")->toString(), "[each [1, 2, 3]]");
}
TEST(ListComprehensions, EachInFor) {
    auto* lc = dynamic_cast<ListComprehension*>(E("[for (i = [0:2]) each [i, i+1]]"));
    ASSERT_NE(lc, nullptr);
    EXPECT_NE(dynamic_cast<ListCompFor*>(lc->elements[0].get()), nullptr);
}
TEST(ListComprehensions, NestedEach) {
    auto* outer = dynamic_cast<ListComprehension*>(E("[each [each [1, 2, 3]]]"));
    ASSERT_NE(outer, nullptr);
    EXPECT_NE(dynamic_cast<ListCompEach*>(outer->elements[0].get()), nullptr);
}
TEST(ListComprehensions, ForIfComprehension) {
    EXPECT_NE(dynamic_cast<ListComprehension*>(E("[for (i = [0:10]) if (i % 2 == 0) i * 2]")), nullptr);
}
TEST(ListComprehensions, ForLetIfComprehension) {
    EXPECT_NE(dynamic_cast<ListComprehension*>(E("[for (i = [0:10]) let(j = i * 2) if (j > 5) j]")), nullptr);
}
TEST(ListComprehensions, ParenListcomp) {
    auto* lc = dynamic_cast<ListComprehension*>(E("[(for (i = [0:3]) i)]"));
    ASSERT_NE(lc, nullptr);
    ASSERT_EQ(lc->elements.size(), 1u);
    EXPECT_NE(dynamic_cast<ListCompFor*>(lc->elements[0].get()), nullptr);
}
TEST(ListComprehensions, TwoForElements) {
    auto* lc = dynamic_cast<ListComprehension*>(E("[for (i = [0:3]) i, for (j = [0:2]) j]"));
    ASSERT_NE(lc, nullptr);
    ASSERT_EQ(lc->elements.size(), 2u);
    for (auto& e : lc->elements) {
        EXPECT_NE(dynamic_cast<ListCompFor*>(e.get()), nullptr);
    }
}
TEST(ListComprehensions, MixedElements) {
    auto* lc = dynamic_cast<ListComprehension*>(E("[1, for (i = [0:2]) i, 3]"));
    ASSERT_NE(lc, nullptr);
    EXPECT_EQ(lc->elements.size(), 3u);
}
TEST(ListComprehensions, ForExpression) {
    auto* lc = dynamic_cast<ListComprehension*>(E("[for (i = [0:5]) i * 2]"));
    ASSERT_NE(lc, nullptr);
    EXPECT_NE(dynamic_cast<ListCompFor*>(lc->elements[0].get()), nullptr);
}
TEST(ListComprehensions, ForNested) {
    EXPECT_NE(dynamic_cast<ListComprehension*>(E("[for (i = [0:5]) [for (j = [0:3]) i + j]]")), nullptr);
}
TEST(ListComprehensions, IfNested) {
    EXPECT_NE(dynamic_cast<ListComprehension*>(E("[for (i = [0:5]) if (i > 0) if (i < 5) i]")), nullptr);
}
TEST(ListComprehensions, LetSimple) {
    EXPECT_NE(dynamic_cast<ListComprehension*>(E("[for (i = [0:5]) let(j = i * 2) j]")), nullptr);
}
TEST(ListComprehensions, LetMultiple) {
    EXPECT_NE(dynamic_cast<ListComprehension*>(E("[for (i = [0:5]) let(j = i * 2, k = j + 1) k]")), nullptr);
}
TEST(ListComprehensions, LetNested) {
    EXPECT_NE(dynamic_cast<ListComprehension*>(E("[for (i = [0:5]) let(j = i * 2) let(k = j + 1) k]")), nullptr);
}
TEST(ListComprehensions, NestedComplex) {
    EXPECT_NE(dynamic_cast<ListComprehension*>(E("[for (i = [0:5]) [for (j = [0:3]) if (i + j > 3) i + j]]")), nullptr);
}
TEST(ListComprehensions, Parentheses) {
    EXPECT_NE(dynamic_cast<ListComprehension*>(E("[for (i = [0:5]) (i * 2)]")), nullptr);
}
TEST(ListComprehensions, NestedParentheses) {
    EXPECT_NE(dynamic_cast<ListComprehension*>(E("[for (i = [0:5]) (for (j = [0:3]) i + j)]")), nullptr);
}
TEST(ListComprehensions, ParenExprAst) {
    auto ast = parseSrc("x = [(for (i = [0:3]) i)];");
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    auto* comp = dynamic_cast<ListComprehension*>(a->expr.get());
    ASSERT_NE(comp, nullptr);
    ASSERT_EQ(comp->elements.size(), 1u);
    EXPECT_NE(dynamic_cast<ListCompFor*>(comp->elements[0].get()), nullptr);
}

// -- ListCompCFor inits/incrs counts -------------------------------------

namespace {
ListCompCFor* cforOf(const std::string& code, std::vector<std::unique_ptr<ASTNode>>& ast) {
    ast = parseSrc("x = " + code + ";");
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    auto* lc = dynamic_cast<ListComprehension*>(a->expr.get());
    return dynamic_cast<ListCompCFor*>(lc->elements[0].get());
}
} // namespace

TEST(ListCompCFor, OneInitOneIncr) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    auto* c = cforOf("[for (i = 0; i < 5; i = i + 1) i]", ast);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->inits.size(), 1u);
    EXPECT_EQ(c->incrs.size(), 1u);
}
TEST(ListCompCFor, TwoInits) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    auto* c = cforOf("[for (i = 0, j = 1; i < 5; i = i + 1) i]", ast);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->inits.size(), 2u);
}
TEST(ListCompCFor, TwoIncrs) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    auto* c = cforOf("[for (i = 0; i < 5; i = i + 1, j = i * 2) i]", ast);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->incrs.size(), 2u);
}
TEST(ListCompCFor, BothZero) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    auto* c = cforOf("[for ( ; i < 5 ; ) i]", ast);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->inits.size(), 0u);
    EXPECT_EQ(c->incrs.size(), 0u);
}
TEST(ListCompCFor, ZeroInits) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    auto* c = cforOf("[for ( ; i < 5 ; i = i + 1) i]", ast);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->inits.size(), 0u);
}
TEST(ListCompCFor, OneInit) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    auto* c = cforOf("[for (i = 0 ; i < 5 ; i = i + 1) i]", ast);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->inits.size(), 1u);
}
TEST(ListCompCFor, ThreeInits) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    auto* c = cforOf("[for (i = 0, j = 1, k = 2 ; i < 5 ; i = i + 1) i]", ast);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->inits.size(), 3u);
}
TEST(ListCompCFor, ZeroIncrs) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    auto* c = cforOf("[for (i = 0 ; i < 5 ; ) i]", ast);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->incrs.size(), 0u);
}
TEST(ListCompCFor, OneIncr) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    auto* c = cforOf("[for (i = 0 ; i < 5 ; i = i + 1) i]", ast);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->incrs.size(), 1u);
}
TEST(ListCompCFor, ThreeIncrs) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    auto* c = cforOf("[for (i = 0 ; i < 5 ; i = i + 1, j = i * 2, k = 3) i]", ast);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->incrs.size(), 3u);
}

// -- ListCompFor assignment counts ---------------------------------------

namespace {
ListCompFor* forOf(const std::string& code, std::vector<std::unique_ptr<ASTNode>>& ast) {
    ast = parseSrc("x = " + code + ";");
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    auto* lc = dynamic_cast<ListComprehension*>(a->expr.get());
    return dynamic_cast<ListCompFor*>(lc->elements[0].get());
}
} // namespace

TEST(ListCompForAssignments, One) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    EXPECT_EQ(forOf("[for (i = [0:5]) i]", ast)->assignments.size(), 1u);
}
TEST(ListCompForAssignments, Two) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    EXPECT_EQ(forOf("[for (i = [0:5], j = [0:3]) i + j]", ast)->assignments.size(), 2u);
}
TEST(ListCompForAssignments, Three) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    EXPECT_EQ(forOf("[for (i = [0:5], j = [0:3], k = [0:2]) i + j + k]", ast)->assignments.size(), 3u);
}

// -- ListCompLet assignment counts ---------------------------------------

namespace {
ListCompLet* letOf(const std::string& code, std::vector<std::unique_ptr<ASTNode>>& ast) {
    ast = parseSrc("x = " + code + ";");
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    auto* lc = dynamic_cast<ListComprehension*>(a->expr.get());
    return dynamic_cast<ListCompLet*>(lc->elements[0].get());
}
} // namespace

TEST(ListCompLetAssignments, One) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    EXPECT_EQ(letOf("[let(a = 1) for (i = [0:3]) a + i]", ast)->assignments.size(), 1u);
}
TEST(ListCompLetAssignments, Two) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    EXPECT_EQ(letOf("[let(a = 1, b = 2) for (i = [0:3]) a + b + i]", ast)->assignments.size(), 2u);
}
TEST(ListCompLetAssignments, Three) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    EXPECT_EQ(letOf("[let(a = 1, b = 2, c = 3) for (i = [0:3]) a + b + c + i]", ast)->assignments.size(), 3u);
}

// -- Vector operations ------------------------------------------------

TEST(VectorOperations, VectorAssignment) {
    auto ast = parseSrc("x = [1, 2, 3];");
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    EXPECT_NE(dynamic_cast<ListComprehension*>(a->expr.get()), nullptr);
}
TEST(VectorOperations, VectorInFunction) {
    EXPECT_NO_THROW(parseSrc("cube([10, 20, 30]);"));
}
TEST(VectorOperations, VectorInExpression) {
    EXPECT_NO_THROW(parseSrc("x = [1, 2, 3] + [4, 5, 6];"));
}
TEST(VectorOperations, VectorAccess) {
    auto ast = parseSrc("x = vec[0];");
    EXPECT_NE(dynamic_cast<Assignment*>(ast[0].get()), nullptr);
}
