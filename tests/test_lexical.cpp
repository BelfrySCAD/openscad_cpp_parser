// Ported from openscad_lalr_parser/tests/test_lexical.py: comments, number
// literals, string literals, booleans, undef, identifiers.
#include "test_helpers.hpp"

#include <gtest/gtest.h>

using namespace oscad;

// -- Comments ---------------------------------------------------------

TEST(LexicalComments, SingleLineComment) {
    EXPECT_NO_THROW(parseSrc("// This is a comment"));
}

TEST(LexicalComments, SingleLineCommentWithCode) {
    auto ast = parseSrc("x = 5; // comment");
    ASSERT_EQ(ast.size(), 1u);
    EXPECT_NE(dynamic_cast<Assignment*>(ast[0].get()), nullptr);
}

TEST(LexicalComments, MultiLineComment) {
    EXPECT_NO_THROW(parseSrc("/* This is a\nmulti-line comment */"));
}

TEST(LexicalComments, MultiLineCommentSingleLine) {
    EXPECT_NO_THROW(parseSrc("/* comment */"));
}

TEST(LexicalComments, CommentsInExpressions) {
    auto ast = parseSrc("x = 1 + /* comment */ 2;");
    ASSERT_EQ(ast.size(), 1u);
    EXPECT_NE(dynamic_cast<Assignment*>(ast[0].get()), nullptr);
}

TEST(LexicalComments, BlockCommentFollowedByBlockComment) {
    EXPECT_NO_THROW(parseSrc("/* comment *//* another comment */"));
}

TEST(LexicalComments, BlockCommentFollowedByInlineComment) {
    EXPECT_NO_THROW(parseSrc("/* comment */// another comment"));
}

TEST(LexicalComments, InlineCommentWithNestedInline) {
    EXPECT_NO_THROW(parseSrc("// comment // the same comment"));
}

TEST(LexicalComments, InlineCommentWithNestedBlockComment) {
    EXPECT_NO_THROW(parseSrc("// comment /* the same comment */"));
}

TEST(LexicalComments, InlineCommentWithNestedUnclosedBlockComment) {
    EXPECT_NO_THROW(parseSrc("// comment /* the same comment"));
}

TEST(LexicalComments, BlockCommentWithUnclosedNestedBlockComment) {
    // Block comments don't nest: the first `*/` closes the comment, so the
    // whole thing is one valid (if oddly-worded) comment.
    EXPECT_NO_THROW(parseSrc("/* comment /* the same comment */"));
}

TEST(LexicalComments, BlockCommentWithNestedBlockCommentFailsToParse) {
    // Because block comments don't nest, the first `*/` closes the comment
    // after "the same comment", leaving a stray trailing `*/` that's a
    // syntax error.
    EXPECT_THROW(parseSrc("/* comment /* the same comment */*/"), ParseError);
}

TEST(LexicalComments, SingleLineCommentWithIncludeComments) {
    auto ast = getASTFromString("// This is a comment", /*includeComments=*/true);
    size_t count = 0;
    for (auto& n : ast) {
        if (n->kind() == NodeKind::CommentLine) {
            ++count;
        }
    }
    EXPECT_GE(count, 1u);
}

TEST(LexicalComments, MultiLineCommentWithIncludeComments) {
    auto ast = getASTFromString("/* block comment */", /*includeComments=*/true);
    size_t count = 0;
    for (auto& n : ast) {
        if (n->kind() == NodeKind::CommentSpan) {
            ++count;
        }
    }
    EXPECT_GE(count, 1u);
}

TEST(LexicalComments, CommentsAtVariousPositions) {
    EXPECT_NO_THROW(parseSrc("// leading comment\nx = 1;"));
    EXPECT_NO_THROW(parseSrc("x = 1; // trailing comment"));
    EXPECT_NO_THROW(parseSrc("x = 1; /* between */ y = 2;"));
    EXPECT_NO_THROW(parseSrc("module test() { // inside\n  cube(1); }"));
    EXPECT_NO_THROW(parseSrc("cube(/* size */ 10);"));
    EXPECT_NO_THROW(parseSrc("module test() { cube(1); // end\n}"));
}

// -- Number literals ----------------------------------------------------

TEST(LexicalNumberLiterals, Integer) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    auto* e = exprSrc("42", ast);
    auto* n = dynamic_cast<NumberLiteral*>(e);
    ASSERT_NE(n, nullptr);
    EXPECT_DOUBLE_EQ(n->val, 42.0);
}

TEST(LexicalNumberLiterals, Float) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    auto* n = dynamic_cast<NumberLiteral*>(exprSrc("3.14", ast));
    ASSERT_NE(n, nullptr);
    EXPECT_DOUBLE_EQ(n->val, 3.14);
}

TEST(LexicalNumberLiterals, ScientificNotation) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    auto* n = dynamic_cast<NumberLiteral*>(exprSrc("1e10", ast));
    ASSERT_NE(n, nullptr);
    EXPECT_DOUBLE_EQ(n->val, 1e10);
}

TEST(LexicalNumberLiterals, ScientificNotationNegativeExponent) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    auto* n = dynamic_cast<NumberLiteral*>(exprSrc("1.5e-3", ast));
    ASSERT_NE(n, nullptr);
    EXPECT_DOUBLE_EQ(n->val, 1.5e-3);
}

TEST(LexicalNumberLiterals, ScientificPositiveExponent) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    auto* n = dynamic_cast<NumberLiteral*>(exprSrc("1e+10", ast));
    ASSERT_NE(n, nullptr);
    EXPECT_DOUBLE_EQ(n->val, 1e10);
}

TEST(LexicalNumberLiterals, Hex) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    auto* n = dynamic_cast<NumberLiteral*>(exprSrc("0xFF", ast));
    ASSERT_NE(n, nullptr);
    EXPECT_DOUBLE_EQ(n->val, 255.0);
}

TEST(LexicalNumberLiterals, HexLowercase) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    auto* n = dynamic_cast<NumberLiteral*>(exprSrc("0xff", ast));
    ASSERT_NE(n, nullptr);
    EXPECT_DOUBLE_EQ(n->val, 255.0);
}

TEST(LexicalNumberLiterals, LeadingDot) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    auto* n = dynamic_cast<NumberLiteral*>(exprSrc(".5", ast));
    ASSERT_NE(n, nullptr);
    EXPECT_DOUBLE_EQ(n->val, 0.5);
}

TEST(LexicalNumberLiterals, NegativeIntegerIsUnaryMinusWrapper) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    auto* e = exprSrc("-42", ast);
    EXPECT_NE(dynamic_cast<UnaryMinusOp*>(e), nullptr);
}

TEST(LexicalNumberLiterals, PositiveIntegerIsPassThrough) {
    // Unary + is a no-op: no wrapper node, just the literal itself.
    std::vector<std::unique_ptr<ASTNode>> ast;
    auto* n = dynamic_cast<NumberLiteral*>(exprSrc("+42", ast));
    ASSERT_NE(n, nullptr);
    EXPECT_DOUBLE_EQ(n->val, 42.0);
}

TEST(LexicalNumberLiterals, NumberStr) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    EXPECT_EQ(exprSrc("42", ast)->toString(), "42");
}

TEST(LexicalNumberLiterals, FloatStr) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    EXPECT_EQ(exprSrc("3.14", ast)->toString(), "3.14");
}

// -- String literals ------------------------------------------------------

TEST(LexicalStringLiterals, SimpleString) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    auto* s = dynamic_cast<StringLiteral*>(exprSrc("\"hello\"", ast));
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->val, "hello");
}

TEST(LexicalStringLiterals, EmptyString) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    auto* s = dynamic_cast<StringLiteral*>(exprSrc("\"\"", ast));
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->val, "");
}

TEST(LexicalStringLiterals, EscapedQuotes) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    auto* s = dynamic_cast<StringLiteral*>(exprSrc(R"("say \"hi\"")", ast));
    EXPECT_NE(s, nullptr);
}

TEST(LexicalStringLiterals, StringWithEscapes) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    auto* s = dynamic_cast<StringLiteral*>(exprSrc(R"("hello\nworld")", ast));
    EXPECT_NE(s, nullptr);
}

TEST(LexicalStringLiterals, StringWithLeadingSpaces) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    auto* s = dynamic_cast<StringLiteral*>(exprSrc("\"  foo\"", ast));
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->val, "  foo");
}

TEST(LexicalStringLiterals, StringWithOnlySpaces) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    auto* s = dynamic_cast<StringLiteral*>(exprSrc("\"   \"", ast));
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->val, "   ");
}

TEST(LexicalStringLiterals, StringStr) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    EXPECT_EQ(exprSrc("\"hello\"", ast)->toString(), "\"hello\"");
}

// -- Boolean literals -----------------------------------------------------

TEST(LexicalBooleanLiterals, True) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    auto* b = dynamic_cast<BooleanLiteral*>(exprSrc("true", ast));
    ASSERT_NE(b, nullptr);
    EXPECT_TRUE(b->val);
}

TEST(LexicalBooleanLiterals, False) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    auto* b = dynamic_cast<BooleanLiteral*>(exprSrc("false", ast));
    ASSERT_NE(b, nullptr);
    EXPECT_FALSE(b->val);
}

TEST(LexicalBooleanLiterals, TrueStr) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    EXPECT_EQ(exprSrc("true", ast)->toString(), "true");
}

TEST(LexicalBooleanLiterals, FalseStr) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    EXPECT_EQ(exprSrc("false", ast)->toString(), "false");
}

// -- undef ------------------------------------------------------------

TEST(LexicalUndefinedLiteral, Undef) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    EXPECT_NE(dynamic_cast<UndefinedLiteral*>(exprSrc("undef", ast)), nullptr);
}

TEST(LexicalUndefinedLiteral, UndefStr) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    EXPECT_EQ(exprSrc("undef", ast)->toString(), "undef");
}

// -- Identifiers ------------------------------------------------------

TEST(LexicalIdentifiers, Simple) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    auto* id = dynamic_cast<Identifier*>(exprSrc("foo", ast));
    ASSERT_NE(id, nullptr);
    EXPECT_EQ(id->name, "foo");
}

TEST(LexicalIdentifiers, UnderscorePrefix) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    EXPECT_EQ(dynamic_cast<Identifier*>(exprSrc("_private", ast))->name, "_private");
}

TEST(LexicalIdentifiers, DollarPrefix) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    EXPECT_EQ(dynamic_cast<Identifier*>(exprSrc("$fn", ast))->name, "$fn");
}

TEST(LexicalIdentifiers, Alphanumeric) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    EXPECT_EQ(dynamic_cast<Identifier*>(exprSrc("myVar123", ast))->name, "myVar123");
}

TEST(LexicalIdentifiers, DoubleUnderscore) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    EXPECT_EQ(dynamic_cast<Identifier*>(exprSrc("__internal", ast))->name, "__internal");
}

TEST(LexicalIdentifiers, DollarUnderscore) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    EXPECT_EQ(dynamic_cast<Identifier*>(exprSrc("$_special", ast))->name, "$_special");
}

TEST(LexicalIdentifiers, UnderscoreInFunctionName) {
    auto ast = parseSrc("function _helper(x) = x + 1;");
    auto* f = dynamic_cast<FunctionDeclaration*>(ast[0].get());
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->name->name, "_helper");
}

TEST(LexicalIdentifiers, UnderscoreInModuleName) {
    auto ast = parseSrc("module _internal() { cube(1); }");
    auto* m = dynamic_cast<ModuleDeclaration*>(ast[0].get());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->name->name, "_internal");
}

TEST(LexicalIdentifiers, SimpleIdentifierAsTarget) {
    auto ast = parseSrc("x = 1;");
    EXPECT_NE(dynamic_cast<Assignment*>(ast[0].get()), nullptr);
}

TEST(LexicalIdentifiers, IdentifierWithUnderscore) {
    auto ast = parseSrc("my_var = 1;");
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->name->name, "my_var");
}

TEST(LexicalIdentifiers, IdentifierWithNumbers) {
    auto ast = parseSrc("var1 = 1;");
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->name->name, "var1");
}

TEST(LexicalIdentifiers, IdentifierDollarSignAsTarget) {
    auto ast = parseSrc("$var = 1;");
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->name->name, "$var");
}

TEST(LexicalIdentifiers, IdentifierMixedCase) {
    auto ast = parseSrc("myVariable = 1;");
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->name->name, "myVariable");
}

TEST(LexicalIdentifiers, IdentifierLeadingUnderscoreAsTarget) {
    auto ast = parseSrc("_private_var = 1;");
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->name->name, "_private_var");
}

TEST(LexicalIdentifiers, IdentifierLeadingUnderscoreUppercase) {
    auto ast = parseSrc("_UNDEF = 1;");
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->name->name, "_UNDEF");
}

TEST(LexicalIdentifiers, IdentifierStr) {
    std::vector<std::unique_ptr<ASTNode>> ast;
    EXPECT_EQ(exprSrc("foo", ast)->toString(), "foo");
}

// -- Source spans ---------------------------------------------------------

namespace {
// The source text a node's own position spans -- what a consumer slicing
// by start/end_offset actually gets back.
std::string spanned(const std::string& src, const oscad::ASTNode& n) {
    const auto& p = n.position();
    return src.substr(static_cast<size_t>(p.start_offset),
                       static_cast<size_t>(p.end_offset - p.start_offset));
}
} // namespace

// A string is matched by SEVERAL lexer rules (opening quote, content runs,
// escapes, closing quote), and YY_USER_ACTION resets tokenStart on every
// one of them -- so building the token's location from currentTokenLoc() at
// the closing quote described just that one character. Every StringLiteral
// spanned a bare `"`, and so did anything wrapping it, which silently
// corrupted a downstream consumer that sliced source by those offsets.
TEST(SourceSpans, StringLiteralSpansTheWholeLiteral) {
    struct Case { std::string src; std::string want; };
    const Case cases[] = {
        {"a = \"txt\";", "\"txt\""},
        {"a = \"\";", "\"\""},                                  // empty
        {"a = \"with \\\"quote\\\" in it\";", "\"with \\\"quote\\\" in it\""},
        {"a = \"esc \\n seq\";", "\"esc \\n seq\""},
        {"a = \"comma,inside\";", "\"comma,inside\""},
    };
    for (const Case& c : cases) {
        auto ast = oscad::parseSrc(c.src);
        ASSERT_EQ(ast.size(), 1u) << c.src;
        auto* assign = dynamic_cast<oscad::Assignment*>(ast[0].get());
        ASSERT_NE(assign, nullptr) << c.src;
        EXPECT_EQ(spanned(c.src, *assign->expr), c.want) << c.src;
    }
}

// The enclosing node's span has to be right too -- that is how the bug
// actually did damage, by dragging an argument's end_offset into the
// middle of the string.
TEST(SourceSpans, NodesWrappingAStringSpanCorrectly) {
    const std::string src = "f(\"x,y\", b);";
    auto ast = oscad::parseSrc(src);
    auto* call = dynamic_cast<oscad::ModularCall*>(ast[0].get());
    ASSERT_NE(call, nullptr);
    ASSERT_EQ(call->arguments.size(), 2u);
    EXPECT_EQ(spanned(src, *call->arguments[0]), "\"x,y\"");
    EXPECT_EQ(spanned(src, *call->arguments[1]), "b");
}

TEST(SourceSpans, StringsInAVectorSpanCorrectly) {
    const std::string src = "x = [\"a,b\",\"c\"];";
    auto ast = oscad::parseSrc(src);
    auto* assign = dynamic_cast<oscad::Assignment*>(ast[0].get());
    ASSERT_NE(assign, nullptr);
    auto* vec = dynamic_cast<oscad::ListComprehension*>(assign->expr.get());
    ASSERT_NE(vec, nullptr);
    ASSERT_EQ(vec->elements.size(), 2u);
    EXPECT_EQ(spanned(src, *vec->elements[0]), "\"a,b\"");
    EXPECT_EQ(spanned(src, *vec->elements[1]), "\"c\"");
}

// Multi-line strings must still report the OPENING quote's line/column, not
// the closing one's.
TEST(SourceSpans, MultiLineStringReportsItsStartingLine) {
    const std::string src = "a = 1;\nb = \"one\ntwo\";\n";
    auto ast = oscad::parseSrc(src);
    ASSERT_EQ(ast.size(), 2u);
    auto* assign = dynamic_cast<oscad::Assignment*>(ast[1].get());
    ASSERT_NE(assign, nullptr);
    EXPECT_EQ(assign->expr->position().line, 2);
    EXPECT_EQ(spanned(src, *assign->expr), "\"one\ntwo\"");
}
