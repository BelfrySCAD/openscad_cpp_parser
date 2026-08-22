#include "openscad_cpp_parser/api.hpp"
#include "openscad_cpp_parser/pretty_print.hpp"

#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace oscad;

namespace {

// Idempotency is a strong, cheap proxy for round-trip correctness: if
// print(parse(src)) reparses to a tree that prints identically again, the
// formatter and parser agree with each other (matches the reference
// library's own round-trip test strategy in test_pretty_print.py).
void expectStablePrint(const std::string& src) {
    auto ast1 = parseAst(src);
    std::string printed1 = toOpenscad(ast1);
    auto ast2 = parseAst(printed1);
    std::string printed2 = toOpenscad(ast2);
    EXPECT_EQ(printed1, printed2) << "not idempotent for source:\n" << src;
    // Re-parsing must also preserve the top-level node count/shape.
    EXPECT_EQ(ast1.size(), ast2.size());
}

// Same idea, but with comment attachment enabled -- exercises the
// terminator-placement fixes around trailing `//` line comments (a `//`
// runs to end-of-line, so a naively-appended `;`/`,`/`)` right after one
// would be silently swallowed into the comment and break re-parsing).
void expectStablePrintWithComments(const std::string& src) {
    auto ast1 = getASTFromString(src, /*includeComments=*/true);
    std::string printed1 = toOpenscad(ast1);
    auto ast2 = getASTFromString(printed1, /*includeComments=*/true);
    std::string printed2 = toOpenscad(ast2);
    EXPECT_EQ(printed1, printed2) << "not idempotent for source:\n" << src << "\n\nfirst print:\n" << printed1;
    EXPECT_EQ(ast1.size(), ast2.size());
}

} // namespace

TEST(PrettyPrint, SimpleAssignment) {
    auto ast = parseAst("x = 42;");
    EXPECT_EQ(toOpenscad(ast), "x = 42;");
}

TEST(PrettyPrint, ModuleDeclaration) {
    expectStablePrint("module box(w, h) { cube([w, h, 1]); }");
}

TEST(PrettyPrint, FunctionDeclaration) {
    expectStablePrint("function add(a, b) = a + b;");
}

TEST(PrettyPrint, IfElse) {
    expectStablePrint("if (true) { cube(1); } else { sphere(1); }");
}

TEST(PrettyPrint, ForLoop) {
    expectStablePrint("for (i = [0:5]) cube(i);");
}

TEST(PrettyPrint, ListComprehension) {
    expectStablePrint("x = [for (i = [0:5]) i * 2];");
}

TEST(PrettyPrint, LetExpression) {
    expectStablePrint("x = let(a = 1, b = a + 1) a + b;");
}

TEST(PrettyPrint, Modifiers) {
    expectStablePrint("#!cube(1);");
}

TEST(PrettyPrint, TernaryChain) {
    expectStablePrint("x = a ? 1 : b ? 2 : 3;");
}

TEST(PrettyPrint, ComplexModel) {
    expectStablePrint(
        "module shelf(width=60, depth=30, thickness=3) {\n"
        "    cube([width, depth, thickness]);\n"
        "    translate([0, 0, thickness]) cube([thickness, depth, 40]);\n"
        "}\n"
        "function vol(w, d, t) = w * d * t;\n"
        "shelf(width=80);\n");
}

TEST(PrettyPrint, LongCallWrapsMultiline) {
    // Force the >80-char multiline path.
    expectStablePrint(
        "translate([100, 200, 300]) rotate([10, 20, 30]) "
        "some_really_long_module_name(alpha=1, beta=2, gamma=3, delta=4, epsilon=5);");
}

TEST(PrettyPrint, TrailingLineCommentOnAssignmentDoesNotSwallowSemicolon) {
    expectStablePrintWithComments("x = 1; // meaning of x\n");
}

TEST(PrettyPrint, TrailingLineCommentOnLastCallArgumentDoesNotSwallowCloseParen) {
    expectStablePrintWithComments("cube(5 // last arg comment\n);\n");
}

TEST(PrettyPrint, TrailingLineCommentMidVectorDoesNotSwallowComma) {
    expectStablePrintWithComments("x = [1, 2 // note\n, 3];\n");
}

TEST(PrettyPrint, TrailingLineCommentOnFunctionBodyDoesNotSwallowSemicolon) {
    expectStablePrintWithComments("function f() = 1; // comment\n");
}

// --- Coverage gap-fill: every binary/unary operator kind ------------------

TEST(PrettyPrint, AllBinaryOperators) {
    const std::vector<std::string> ops = {"+", "-", "*", "/", "%", "^", "&", "|", "<<", ">>",
                                           "&&", "||", "==", "!=", ">", ">=", "<", "<="};
    for (const auto& op : ops) {
        expectStablePrint("x = a " + op + " b;");
    }
}

TEST(PrettyPrint, AllUnaryOperators) {
    expectStablePrint("x = -a;");
    expectStablePrint("x = !a;");
    expectStablePrint("x = ~a;");
}

TEST(PrettyPrint, BinaryOpWithMultilineLeftOperand) {
    std::string out = toOpenscad(parseAst(
        "x = [very_long_element_name_a, very_long_element_name_b, very_long_element_name_c, very_long_element_name_d] + y;"));
    EXPECT_EQ(out.rfind("x = [\n", 0), 0u);
    EXPECT_NE(out.find("] + y;"), std::string::npos);
}

// --- Coverage gap-fill: expression forms besides plain literals/calls -----

TEST(PrettyPrint, BareLetEchoAssertFunctionLiteralExpressions) {
    expectStablePrint("x = let(a = 1) a;");
    expectStablePrint("x = echo(\"hi\") 1;");
    expectStablePrint("x = assert(true) 1;");
    expectStablePrint("x = function(a) a;");
}

TEST(PrettyPrint, PrimaryCallIndexMemberExpressions) {
    expectStablePrint("x = foo(1);");
    expectStablePrint("x = v[0];");
    expectStablePrint("x = v.x;");
}

TEST(PrettyPrint, RangeLiteralWithStep) {
    expectStablePrint("x = [1:2:5];");
}

// --- Coverage gap-fill: list-comprehension clause formatting --------------

TEST(PrettyPrint, ListCompCForShortHeaderStaysOneLine) {
    std::string out = toOpenscad(parseAst("x = [for (i = 0; i < 3; i = i + 1) i];"));
    EXPECT_NE(out.find("for (i = 0; i < 3; i = i + 1)\n"), std::string::npos);
    EXPECT_EQ(out.find("for (\n"), std::string::npos);
}

TEST(PrettyPrint, ListCompCForLongHeaderWraps) {
    expectStablePrint(
        "x = [for (i = 0; i < very_long_condition_limit_value_maximum_xx; i = i + step_increment_value) i];");
}

TEST(PrettyPrint, ListCompLetFormatting) {
    // `let(...)` immediately followed by another comprehension clause
    // parses as ListCompLet, not a bare LetOp list element (below).
    expectStablePrint("x = [let(a = 1) for (i = [1:3]) i + a];");
    expectStablePrint("x = [let(a = 1, b = 2) for (i = [1:3]) i + a + b];");
}

TEST(PrettyPrint, LetAsListElementFormatting) {
    expectStablePrint("x = [let(a = 1) a];");
    expectStablePrint("x = [let(a = 1, b = 2) a + b];");
    expectStablePrint("x = [let(a = 1) very_long_variable_name_alpha_beta_gamma_delta_epsilon_zeta_theta + a];");
}

TEST(PrettyPrint, NestedListComprehensionElement) {
    expectStablePrint("x = [each [for (i = [1:3]) i]];");
}

TEST(PrettyPrint, ListCompIfIfElseEach) {
    expectStablePrint("x = [for (i = [0:9]) if (i % 2 == 0) i];");
    expectStablePrint("x = [for (i = [0:9]) if (i % 2 == 0) i else -i];");
    expectStablePrint("x = [each [1, 2, 3]];");
}

// --- Coverage gap-fill: statement-form multiline wrapping ------------------

TEST(PrettyPrint, ModularForIntersectionForMultilineWrap) {
    expectStablePrint("for (very_long_variable_name_alpha = [0:100], very_long_variable_name_beta = [0:50]) cube(1);");
    expectStablePrint(
        "intersection_for (very_long_variable_name_alpha = [0:100], very_long_variable_name_beta = [0:50]) cube(1);");
}

TEST(PrettyPrint, ModularLetMultiAssignmentWraps) {
    std::string out = toOpenscad(parseAst("let (x = 1, y = 2) cube(1);"));
    EXPECT_EQ(out, "let (\n    x = 1,\n    y = 2\n)\n    cube(1);");
}

TEST(PrettyPrint, ModularEchoAssertLongArgsWrap) {
    expectStablePrint(
        "echo(long_arg_a, long_arg_b, long_arg_c, long_arg_d, long_arg_e, long_arg_f, long_arg_g, long_arg_h) cube(1);");
    expectStablePrint(
        "assert(long_arg_a, long_arg_b, long_arg_c, long_arg_d, long_arg_e, long_arg_f, long_arg_g, long_arg_h) cube(1);");
}

TEST(PrettyPrint, ModularIfWithoutElse) {
    expectStablePrint("if (true) cube(1);");
}

TEST(PrettyPrint, ModularIfElseStatement) {
    expectStablePrint("if (true) cube(1); else cube(2);");
}

TEST(PrettyPrint, AllModifierPrefixes) {
    expectStablePrint("!cube(1);");
    expectStablePrint("#cube(1);");
    expectStablePrint("%cube(1);");
    expectStablePrint("*cube(1);");
}

TEST(PrettyPrint, FunctionDeclarationLongParametersWrap) {
    expectStablePrint(
        "function f(very_long_param_alpha, very_long_param_beta, very_long_param_gamma, very_long_param_delta) = 1;");
}

TEST(PrettyPrint, ModuleDeclarationLongParametersWrap) {
    expectStablePrint(
        "module m(very_long_param_alpha, very_long_param_beta, very_long_param_gamma, very_long_param_delta) { cube(1); }");
}

TEST(PrettyPrint, LongAssignmentRhsWrapsToNewLine) {
    std::string out =
        toOpenscad(parseAst("some_very_long_variable_name_xxxxxxxxxx = another_long_expression_value_yyyyyyy + 1;"));
    auto nl = out.find('\n');
    ASSERT_NE(nl, std::string::npos);
    EXPECT_EQ(out.substr(0, nl).back(), '=');
}

TEST(PrettyPrint, StandaloneTopLevelCommentsAndBlankLines) {
    expectStablePrintWithComments("// standalone\ncube(1);\n\nsphere(1);\n");
}

// --- Coverage gap-fill: inline comments across every classifyNode branch --

TEST(PrettyPrint, InlineCommentsAcrossManyConstructs) {
    expectStablePrintWithComments("x = /* c */ 1 + 2;\n");
    expectStablePrintWithComments("x = -/* c */ 1;\n");
    expectStablePrintWithComments("x = !/* c */ true;\n");
    expectStablePrintWithComments("x = ~/* c */ 1;\n");
    expectStablePrintWithComments("x = [/* s */ 1 : /* e */ 5];\n");
    expectStablePrintWithComments("function f(x = /* d */ 1) = x;\n");
    expectStablePrintWithComments("x = let(a = /* c */ 1) a;\n");
    expectStablePrintWithComments("x = echo(/* c */ \"hi\") 1;\n");
    expectStablePrintWithComments("x = assert(/* c */ true) 1;\n");
    expectStablePrintWithComments("x = function(a = /* d */ 1) a;\n");
    expectStablePrintWithComments("x = foo(/* c */ 1);\n");
    expectStablePrintWithComments("x = v[/* c */ 0];\n");
    expectStablePrintWithComments("x = /* c */ v.x;\n");
    expectStablePrintWithComments("x = [let(a = /* c */ 1) a];\n");
    expectStablePrintWithComments("x = [each /* c */ [1, 2]];\n");
    expectStablePrintWithComments("x = [for (i = /* c */ [1:3]) i];\n");
    expectStablePrintWithComments("x = [for (i = 0; i < /* c */ 3; i = i + 1) i];\n");
    expectStablePrintWithComments("x = [if (/* c */ true) 1];\n");
    expectStablePrintWithComments("x = [if (true) 1 else /* c */ 2];\n");
    expectStablePrintWithComments("for (i = /* c */ [1:3]) cube(1);\n");
    expectStablePrintWithComments("intersection_for (i = /* c */ [1:3]) cube(1);\n");
    expectStablePrintWithComments("let (a = /* c */ 1) cube(a);\n");
    expectStablePrintWithComments("echo(/* c */ 1) cube(1);\n");
    expectStablePrintWithComments("assert(/* c */ true) cube(1);\n");
    expectStablePrintWithComments("if (/* c */ true) cube(1);\n");
    expectStablePrintWithComments("!cube(/* c */ 1);\n");
    expectStablePrintWithComments("#cube(/* c */ 1);\n");
    expectStablePrintWithComments("%cube(/* c */ 1);\n");
    expectStablePrintWithComments("*cube(/* c */ 1);\n");
}

TEST(PrettyPrint, MultipleLeadingLineCommentsOnExpression) {
    // Two leading `//` comments, each on its own argument (both classified
    // inline since something precedes each on its own line) -- exercises
    // the multi-comment leading-line-comment path in fmtCommentedExpr, not
    // just a single one.
    //
    // This used to NOT be idempotent: fmtMultilineArgsGeneric printed each
    // leading line-comment on its own fresh line ("foo(\n    a,\n    //
    // one\n    b,\n..."), which re-parses wrong -- a `//` comment with
    // nothing before it on its own line is classified *standalone*, not
    // inline, so re-parsing split it off into its own top-level node
    // instead of keeping it attached to `b`. Fixed by moving a leading
    // "//..."  line onto the end of the *previous* argument's own line
    // instead (mirrors how fmtListComprehension already handles the same
    // situation for list elements) -- see fmtMultilineArgsGeneric's own
    // comment for the full story.
    expectStablePrintWithComments("foo(a // one\n, b // two\n, c);\n");
    std::string out = toOpenscad(getASTFromString("foo(a // one\n, b // two\n, c);\n", /*includeComments=*/true));
    EXPECT_NE(out.find("a,  // one"), std::string::npos);
    EXPECT_NE(out.find("b,  // two"), std::string::npos);
}

TEST(PrettyPrint, TernaryNestedInTrueBranch) {
    expectStablePrint("x = a ? (b ? c : d) : e;");
}

TEST(PrettyPrint, TernaryCommentBeforeNestedFalseBranch) {
    expectStablePrintWithComments("x = c1 ? a : /* mid */ c2 ? b : d;\n");
}

TEST(PrettyPrint, LetBlockWithListCompBodyCoalescesParenBracket) {
    std::string out = toOpenscad(parseAst("x = let(a = 1, b = 2) [for (i = [0:5]) i + a];"));
    EXPECT_NE(out.find(") [\n"), std::string::npos);
}

// --- Coverage gap-fill, round 2: remaining pretty_print.cpp branches ------

TEST(PrettyPrint, BareEchoAssertExpressionsWithNoTrailingBody) {
    // echo()/assert() used as a *value* with no trailing expression --
    // the body defaults to UndefinedLiteral, taking the short-circuit
    // "no body" branch instead of appending a body expression.
    expectStablePrint("x = echo(1);");
    expectStablePrint("x = assert(true);");
}

TEST(PrettyPrint, PrimaryCallExpressionWrapsWhenTrailingLineCommentForcesIt) {
    // A short call-as-expression whose last argument carries a trailing
    // `//` comment must still take the multiline path (the inline form
    // would silently swallow the closing `)` into the comment).
    std::string out = toOpenscad(getASTFromString("x = foo(5 // last arg comment\n);\n", /*includeComments=*/true));
    EXPECT_NE(out.find("foo(\n"), std::string::npos);
    EXPECT_NE(out.find("// last arg comment"), std::string::npos);
}

TEST(PrettyPrint, LongPrimaryCallExpressionWraps) {
    expectStablePrint(
        "x = some_really_long_function_name(alpha=1, beta=2, gamma=3, delta=4, epsilon=5, zeta=6);");
}

// ParameterDeclaration::leadingComments/trailingComments and
// FunctionDeclaration/ModuleDeclaration's preNameComments/postNameComments/
// postParamsComments (rendered by fmtParameter/joinComments) are real AST
// fields -- declared, JSON-serialized, and read by the pretty-printer --
// but nothing in this port's comment-attachment pipeline (comments.cpp/
// inline_comment_attach.cpp) ever actually populates them: neither is
// listed in classifyNode's switch, so a comment textually positioned near
// a parameter or the function/module name instead falls through and
// attaches to the function/module's own body expression, same as any
// other "couldn't match a nearby field" comment. Confirmed via direct AST
// inspection (`openscad-cpp-parser -j -C` on both scripts below). Not a
// bug introduced here and out of scope to fix (matching real per-parameter/
// per-name comment placement -- mirroring whatever the reference actually
// does -- is a real feature gap, not a coverage gap); documented via the
// actual observed behavior rather than asserting the field population
// that doesn't happen. joinComments'/fmtParameter's own leading/trailing-
// comment loops stay permanently unreachable as a result.
TEST(PrettyPrint, ParameterAdjacentCommentAttachesToParameter) {
    // Used to fall through to the function BODY's own leading comment
    // (ParameterDeclaration.leadingComments was declared, serialized, and
    // rendered, but never populated by the comment-attachment pipeline --
    // addParameterExprList only treats a parameter as a wrappable
    // Expression slot when it HAS a default value, so a bare `x` was never
    // even a candidate). Fixed by claimDeclSignatureComments
    // (inline_comment_attach.cpp), which claims signature-gap comments
    // before the generic exprField-based mechanism ever sees them.
    expectStablePrintWithComments("function f(/* lead */ x) = x;\n");
    std::string out = toOpenscad(getASTFromString("function f(/* lead */ x) = x;\n", /*includeComments=*/true));
    EXPECT_NE(out.find("(/* lead */ x)"), std::string::npos);
}

TEST(PrettyPrint, DeclarationNameAdjacentCommentAttachesNearName) {
    // Used to fall through to the function body for the same reason as
    // above -- preNameComments/postNameComments/postParamsComments were
    // also declared-but-never-populated. This exercises postNameComments
    // specifically (a comment between the name and '(' -- claimed via a
    // raw-text scan for the '(' token, since the grammar doesn't capture
    // its own position).
    expectStablePrintWithComments("function f /* post-name */ (x) = x;\n");
    std::string out = toOpenscad(getASTFromString("function f /* post-name */ (x) = x;\n", /*includeComments=*/true));
    EXPECT_NE(out.find("function f /* post-name */("), std::string::npos);
}

TEST(PrettyPrint, PreNameCommentAttachesBeforeName) {
    expectStablePrintWithComments("function /* pre */ f(x) = x;\n");
    std::string out = toOpenscad(getASTFromString("function /* pre */ f(x) = x;\n", /*includeComments=*/true));
    EXPECT_NE(out.find("function /* pre */ f"), std::string::npos);
}

TEST(PrettyPrint, PostParamsCommentAttachesAfterCloseParen) {
    expectStablePrintWithComments("module m(x) /* post-params */ { cube(x); }\n");
    std::string out = toOpenscad(getASTFromString("module m(x) /* post-params */ { cube(x); }\n", /*includeComments=*/true));
    EXPECT_NE(out.find(") /* post-params */"), std::string::npos);
}

TEST(PrettyPrint, TrailingCommentOnLastParameterAttachesToParameter) {
    expectStablePrintWithComments("module m(x /* trail */) { cube(x); }\n");
    std::string out = toOpenscad(getASTFromString("module m(x /* trail */) { cube(x); }\n", /*includeComments=*/true));
    EXPECT_NE(out.find("x /* trail */)"), std::string::npos);
}

TEST(PrettyPrint, CommentInEmptyParameterListAttachesToPostParams) {
    // No parameter exists to own this comment, so it's folded into
    // postParamsComments (rendered after the closing paren) rather than
    // some dedicated "inside empty parens" slot -- see
    // claimDeclSignatureComments' own ponytail comment.
    expectStablePrintWithComments("module m(/* empty */) { cube(1); }\n");
    std::string out = toOpenscad(getASTFromString("module m(/* empty */) { cube(1); }\n", /*includeComments=*/true));
    EXPECT_NE(out.find("m() /* empty */ {"), std::string::npos);
}

TEST(PrettyPrint, CommentBetweenTwoParametersAttachesToSecond) {
    expectStablePrintWithComments("module m(x, /* mid */ y) { cube(x + y); }\n");
    std::string out = toOpenscad(getASTFromString("module m(x, /* mid */ y) { cube(x + y); }\n", /*includeComments=*/true));
    EXPECT_NE(out.find(", /* mid */ y"), std::string::npos);
}

TEST(PrettyPrint, ListCompForLongAssignmentsWrap) {
    expectStablePrint(
        "x = [for (very_long_variable_name_alpha = [0:100], very_long_variable_name_beta = [0:50]) "
        "very_long_variable_name_alpha];");
}

TEST(PrettyPrint, ModularForBlockWithMultipleChildren) {
    expectStablePrint("for (i = [0:3]) { cube(1); sphere(1); }");
}

TEST(PrettyPrint, EmptyModuleBody) {
    expectStablePrint("module m() {}");
}

TEST(PrettyPrint, AssignmentAsControlFlowChild) {
    expectStablePrint("for (i = [0:3]) { x = i; cube(x); }");
}

TEST(PrettyPrint, StandaloneBlockCommentAtTopLevel) {
    expectStablePrintWithComments("/* standalone */\ncube(1);\n");
}

TEST(PrettyPrint, BlankLinePreservedBetweenCommentBlocks) {
    // BlankLine nodes are only ever produced *between consecutive
    // single-line comment blocks* -- not between arbitrary statements.
    expectStablePrintWithComments("// one\n\n// two\ncube(1);\n");
}

TEST(PrettyPrint, LeadingCommentSpanThenCommentLineOnListElement) {
    // A list element with both a leading CommentSpan *and* a leading
    // CommentLine (CommentSpan first) -- exercises splitLcs's "rest
    // non-empty" branch, distinct from the CommentLine-only case already
    // covered by InlineCommentsAcrossManyConstructs.
    expectStablePrintWithComments(
        "x = [very_long_element_name_a, /* two */ // one\nvery_long_element_name_b, very_long_element_name_c, "
        "very_long_element_name_d];\n");
}

TEST(PrettyPrint, ForLoopAssignmentEndingWithLineCommentForcesWrap) {
    // A short `for (...)` header whose single assignment's formatted value
    // itself ends with a `//` comment must still wrap (anyEndsWithLineComment),
    // not just when the header is over the length limit.
    std::string out = toOpenscad(getASTFromString("for (i = 1 // comment\n) cube(1);\n", /*includeComments=*/true));
    EXPECT_NE(out.find("for (\n"), std::string::npos);
    EXPECT_NE(out.find("// comment"), std::string::npos);
}

TEST(PrettyPrint, LeadingLineCommentFollowedByAnotherLeadingComment) {
    // A leading CommentLine that is *not* the last leading comment (a
    // CommentSpan follows it, still before the value) -- exercises the
    // "inline_part after the last CommentLine" loop in fmtCommentedExpr,
    // distinct from the single-leading-comment case elsewhere.
    std::string out =
        toOpenscad(getASTFromString("x = // one\n/* two */ 1 /* three */;\n", /*includeComments=*/true));
    EXPECT_NE(out.find("// one"), std::string::npos);
    EXPECT_NE(out.find("/* two */"), std::string::npos);
    EXPECT_NE(out.find("/* three */"), std::string::npos);
}

TEST(PrettyPrint, LeadingCommentLineAndTrailingCommentOnLastListElement) {
    // Only the *last* element in a list/args can receive a trailing
    // comment (anything between two non-last elements attaches leading to
    // the next one instead -- see LeadingCommentSpanThenCommentLineOnListElement's
    // own comment for the general shape). Combining a leading CommentLine
    // with a trailing comment needs the comment-bearing element to be last.
    expectStablePrintWithComments(
        "x = [very_long_element_name_a, very_long_element_name_b, very_long_element_name_c, // one\n"
        "very_long_element_name_d /* two */];\n");
}

TEST(PrettyPrint, LeadingCommentOnFirstListElement) {
    // A leading CommentLine on the *first* element (nothing yet accumulated
    // in `lines` to append it onto) takes a different branch than a later
    // element's leading comment.
    std::string out = toOpenscad(getASTFromString(
        "x = [ // header\nvery_long_element_name_a, very_long_element_name_b, very_long_element_name_c, "
        "very_long_element_name_d];\n",
        /*includeComments=*/true));
    EXPECT_NE(out.find("// header"), std::string::npos);
    EXPECT_EQ(out.rfind("x = [\n", 0), 0u);
}

TEST(PrettyPrint, AssignmentRhsWithLeadingCommentAndTrailingLineComment) {
    // fmtValueBeforeTerminator's own leadingPart loop -- needs a CommentedExpr
    // with *both* a leading comment (any kind) and a trailing CommentLine
    // (the condition that routes through this function's special terminator-
    // placement path at all).
    std::string out = toOpenscad(getASTFromString("x = /* lead */ 1 // trail\n;\n", /*includeComments=*/true));
    EXPECT_NE(out.find("/* lead */"), std::string::npos);
    EXPECT_NE(out.find("// trail"), std::string::npos);
}

// Note: fmtNode's/fmtInst's/fmtArgument's own final node.toString()
// fallbacks (and isModuleInstantiationKind's `default: return false;`) are
// genuinely unreachable through real parsing -- every concrete node kind
// that can appear where each is called is already handled explicitly
// upstream -- but unlike the equivalent Python-side fallbacks, these can't
// be exercised by a direct call either: fmtNode/fmtInst/fmtArgument/etc.
// all live in this file's own anonymous namespace (internal linkage), so
// no other translation unit -- including this test file -- can name them.
// Left as a known, permanent coverage ceiling rather than weakening the
// encapsulation just to reach 100%.

// The two-argument range keeps its shape through a print/reparse cycle.
// It would be easy to always print the synthesized step, and the result
// would still be a correct program -- but `[5:0]` and `[5:1:0]` mean
// different things to the evaluator's backwards-range warning, so
// reformatting a file must not quietly convert one into the other.
TEST(PrettyPrint, ImplicitRangeStepSurvivesRoundTrip) {
    auto ast = parseAst("a = [5:0];\nb = [5:1:0];\n");
    const std::string printed = toOpenscad(ast);
    EXPECT_NE(printed.find("[5 : 0]"), std::string::npos) << printed;
    EXPECT_NE(printed.find("[5 : 1 : 0]"), std::string::npos) << printed;
    expectStablePrint("a = [5:0];\nb = [5:1:0];\n");
}
