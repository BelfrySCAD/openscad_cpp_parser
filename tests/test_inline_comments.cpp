#include "openscad_cpp_parser/api.hpp"
#include "openscad_cpp_parser/serialization.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using namespace oscad;

TEST(InlineComments, TrailingLineCommentWrapsAssignmentExpr) {
    auto ast = getASTFromString("x = 1; // meaning\n", /*includeComments=*/true);
    ASSERT_EQ(ast.size(), 1u);
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    auto* wrapped = dynamic_cast<CommentedExpr*>(a->expr.get());
    ASSERT_NE(wrapped, nullptr);
    ASSERT_EQ(wrapped->trailingComments.size(), 1u);
    EXPECT_EQ(wrapped->trailingComments[0]->kind(), NodeKind::CommentLine);
    auto* inner = dynamic_cast<NumberLiteral*>(wrapped->expr.get());
    ASSERT_NE(inner, nullptr);
    EXPECT_DOUBLE_EQ(inner->val, 1.0);
}

TEST(InlineComments, LeadingBlockCommentWrapsArgument) {
    auto ast = getASTFromString("cube(/* size */ 5);\n", /*includeComments=*/true);
    ASSERT_EQ(ast.size(), 1u);
    auto* call = dynamic_cast<ModularCall*>(ast[0].get());
    ASSERT_NE(call, nullptr);
    ASSERT_EQ(call->arguments.size(), 1u);
    auto* posArg = dynamic_cast<PositionalArgument*>(call->arguments[0].get());
    ASSERT_NE(posArg, nullptr);
    auto* wrapped = dynamic_cast<CommentedExpr*>(posArg->expr.get());
    ASSERT_NE(wrapped, nullptr);
    ASSERT_EQ(wrapped->leadingComments.size(), 1u);
    EXPECT_EQ(wrapped->leadingComments[0]->kind(), NodeKind::CommentSpan);
}

TEST(InlineComments, CommentBetweenVectorElementsAttachesToNearestExpr) {
    auto ast = getASTFromString("x = [1, /* mid */ 2, 3];\n", /*includeComments=*/true);
    ASSERT_EQ(ast.size(), 1u);
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    auto* vec = dynamic_cast<ListComprehension*>(a->expr.get());
    ASSERT_NE(vec, nullptr);
    ASSERT_EQ(vec->elements.size(), 3u);
    // element[0] is a plain NumberLiteral (nothing precedes it to attach to)
    EXPECT_EQ(vec->elements[0]->kind(), NodeKind::NumberLiteral);
    // element[1] should be wrapped since the comment precedes it
    auto* wrapped = dynamic_cast<CommentedExpr*>(vec->elements[1].get());
    ASSERT_NE(wrapped, nullptr);
    EXPECT_EQ(wrapped->leadingComments.size(), 1u);
}

TEST(InlineComments, NoCommentsWithoutFlagStaysUnwrapped) {
    auto ast = getASTFromString("x = 1; // meaning\n", /*includeComments=*/false);
    ASSERT_EQ(ast.size(), 1u);
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->expr->kind(), NodeKind::NumberLiteral);
}

TEST(InlineComments, ModuleNameIsNotWrapped) {
    // Declarative-identity fields (names) are deliberately never wrap
    // targets -- the comment should fall through to a nearby expression
    // (or be dropped) rather than corrupt the module's own `name` field.
    auto ast = getASTFromString("module foo(/* w */ w) { cube(w); }\n", /*includeComments=*/true);
    ASSERT_EQ(ast.size(), 1u);
    auto* decl = dynamic_cast<ModuleDeclaration*>(ast[0].get());
    ASSERT_NE(decl, nullptr);
    EXPECT_EQ(decl->name->kind(), NodeKind::Identifier);
}

TEST(InlineComments, RoundTripThroughJsonPreservesCommentedExpr) {
    auto ast = getASTFromString("x = 1; // meaning\n", /*includeComments=*/true);
    auto j = astToJson(ast);
    auto rebuilt = astFromJsonArray(j);
    ASSERT_EQ(rebuilt.size(), 1u);
    auto* a = dynamic_cast<Assignment*>(rebuilt[0].get());
    ASSERT_NE(a, nullptr);
    auto* wrapped = dynamic_cast<CommentedExpr*>(a->expr.get());
    ASSERT_NE(wrapped, nullptr);
    ASSERT_EQ(wrapped->trailingComments.size(), 1u);
}

// --- Coverage gap-fill: classifyNode branches not exercised above ---------

TEST(InlineComments, NamedArgumentValueWrapped) {
    // addArgumentExprList's NamedArgument branch specifically -- every
    // comment test above uses a positional argument.
    auto ast = getASTFromString("x = foo(a=/* c */ 1);\n", /*includeComments=*/true);
    ASSERT_EQ(ast.size(), 1u);
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    auto* call = dynamic_cast<PrimaryCall*>(a->expr.get());
    ASSERT_NE(call, nullptr);
    ASSERT_EQ(call->arguments.size(), 1u);
    auto* namedArg = dynamic_cast<NamedArgument*>(call->arguments[0].get());
    ASSERT_NE(namedArg, nullptr);
    auto* wrapped = dynamic_cast<CommentedExpr*>(namedArg->expr.get());
    ASSERT_NE(wrapped, nullptr);
    EXPECT_EQ(wrapped->leadingComments.size(), 1u);
}

TEST(InlineComments, ListCompLetWithComment) {
    // `let(...)` immediately followed by another comprehension clause
    // parses as ListCompLet (classifyNode's own dedicated case), not the
    // bare-LetOp-as-list-element shape every other let-in-a-list test here
    // uses.
    auto ast = getASTFromString("x = [let(a = /* c */ 1) for (i = [1:3]) i + a];\n", /*includeComments=*/true);
    ASSERT_EQ(ast.size(), 1u);
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    auto* vec = dynamic_cast<ListComprehension*>(a->expr.get());
    ASSERT_NE(vec, nullptr);
    ASSERT_EQ(vec->elements.size(), 1u);
    auto* letElem = dynamic_cast<ListCompLet*>(vec->elements[0].get());
    ASSERT_NE(letElem, nullptr);
    ASSERT_EQ(letElem->assignments.size(), 1u);
    auto* wrapped = dynamic_cast<CommentedExpr*>(letElem->assignments[0]->expr.get());
    ASSERT_NE(wrapped, nullptr);
    EXPECT_EQ(wrapped->leadingComments.size(), 1u);
}

TEST(InlineComments, ModularIfElseConditionWrapped) {
    auto ast = getASTFromString("if (/* c */ true) cube(1); else cube(2);\n", /*includeComments=*/true);
    ASSERT_EQ(ast.size(), 1u);
    auto* ifElse = dynamic_cast<ModularIfElse*>(ast[0].get());
    ASSERT_NE(ifElse, nullptr);
    auto* wrapped = dynamic_cast<CommentedExpr*>(ifElse->condition.get());
    ASSERT_NE(wrapped, nullptr);
    EXPECT_EQ(wrapped->leadingComments.size(), 1u);
}

TEST(InlineComments, CommentFreeSiblingSubtreeDoesNotCrashRecursion) {
    // A module with two children: the first subtree (translate->cube) has
    // no comment in it at all (walkAttach's !hasRelevant fast path, which
    // still needs to recurse into non-expression children to reach the
    // second subtree, where the comment actually is).
    auto ast = getASTFromString("module m() { translate([1,2,3]) cube(1); sphere(/* c */ 2); }\n",
                                 /*includeComments=*/true);
    ASSERT_EQ(ast.size(), 1u);
    auto* decl = dynamic_cast<ModuleDeclaration*>(ast[0].get());
    ASSERT_NE(decl, nullptr);
    ASSERT_EQ(decl->children.size(), 2u);
    auto* sphere = dynamic_cast<ModularCall*>(decl->children[1].get());
    ASSERT_NE(sphere, nullptr);
    ASSERT_EQ(sphere->arguments.size(), 1u);
    auto* wrapped = dynamic_cast<CommentedExpr*>(dynamic_cast<PositionalArgument*>(sphere->arguments[0].get())->expr.get());
    ASSERT_NE(wrapped, nullptr);
    EXPECT_EQ(wrapped->leadingComments.size(), 1u);
}

TEST(InlineComments, TrailingCommentAfterUseStatementFallsThroughWithoutCrashing) {
    // attachTrailingToLastExpr's own "no wrappable expression field at all"
    // no-op branch -- UseStatement has no Expression field (classifyNode's
    // no-op case group), so a comment whose nearest preceding node is a
    // UseStatement can't attach anywhere and is simply dropped.
    auto ast = getASTFromString("use <foo.scad> // trail\ncube(1);\n", /*includeComments=*/true);
    ASSERT_EQ(ast.size(), 2u);
    EXPECT_NE(dynamic_cast<UseStatement*>(ast[0].get()), nullptr);
    EXPECT_NE(dynamic_cast<ModularCall*>(ast[1].get()), nullptr);
}

TEST(InlineComments, TwoStatementsEachWithOwnCommentSkipsAlreadyConsumedSlot) {
    // walkAttach's own per-comment classification loop re-scans the *whole*
    // shared comments vector at every node it visits -- so once the first
    // statement's own comment is consumed (moved out, leaving a null
    // entry), the second statement's walkAttach pass must skip that null
    // slot (rather than dereferencing it) while also skipping the first
    // statement's own comment on ITS first pass for being out of that
    // statement's span. A single-comment-per-statement test doesn't
    // exercise either "skip" branch; two statements with two separate
    // comments does.
    auto ast = getASTFromString("x = /* a */ 1;\ny = foo(/* b */ 2);\n", /*includeComments=*/true);
    ASSERT_EQ(ast.size(), 2u);
    auto* x = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(x, nullptr);
    auto* xWrapped = dynamic_cast<CommentedExpr*>(x->expr.get());
    ASSERT_NE(xWrapped, nullptr);
    EXPECT_EQ(xWrapped->leadingComments.size(), 1u);

    auto* y = dynamic_cast<Assignment*>(ast[1].get());
    ASSERT_NE(y, nullptr);
    auto* call = dynamic_cast<PrimaryCall*>(y->expr.get());
    ASSERT_NE(call, nullptr);
    auto* posArg = dynamic_cast<PositionalArgument*>(call->arguments[0].get());
    ASSERT_NE(posArg, nullptr);
    auto* yWrapped = dynamic_cast<CommentedExpr*>(posArg->expr.get());
    ASSERT_NE(yWrapped, nullptr);
    EXPECT_EQ(yWrapped->leadingComments.size(), 1u);
}
