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
