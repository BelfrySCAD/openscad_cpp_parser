// `render()` in EXPRESSION position: `obj = render() { cube(1); };`
//
// The statement form (`render() cube(1);`) is deliberately UNCHANGED -- it
// still parses to a plain ModularCall named "render", so everything
// downstream (builtin dispatch, the argument allowlist, json_io) sees what
// it always saw. Only the expression form is new, and it exists at all
// because LALR(1) cannot otherwise tell `render(` in expression position
// apart from a function call -- hence `render` being a reserved keyword.

#include "openscad_cpp_parser/api.hpp"
#include "openscad_cpp_parser/pretty_print.hpp"
#include "openscad_cpp_parser/serialization.hpp"
#include "test_helpers.hpp"

#include <gtest/gtest.h>

using namespace oscad;

namespace {

const RenderExpression* renderExprOf(const std::vector<std::unique_ptr<ASTNode>>& ast, size_t idx = 0) {
    auto* a = dynamic_cast<Assignment*>(ast[idx].get());
    return a ? dynamic_cast<const RenderExpression*>(a->expr.get()) : nullptr;
}

} // namespace

// -- The expression form --------------------------------------------------

TEST(RenderExpression, ParsesInAssignment) {
    auto ast = parseSrc("obj = render() { cube(1); };");
    ASSERT_EQ(ast.size(), 1u);
    const RenderExpression* r = renderExprOf(ast);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->kind(), NodeKind::RenderExpression);
    EXPECT_TRUE(r->arguments.empty());
    ASSERT_EQ(r->children.size(), 1u);
    EXPECT_EQ(r->children[0]->kind(), NodeKind::ModularCall);
}

TEST(RenderExpression, ParsesTheOriginalMotivatingExample) {
    // A trailing `}` child needs no `;` of its own, so this form -- unlike a
    // bare call -- terminates its enclosing assignment cleanly.
    auto ast = parseSrc("obj = render() difference() { cube(100); sphere(20); };");
    ASSERT_EQ(ast.size(), 1u);
    const RenderExpression* r = renderExprOf(ast);
    ASSERT_NE(r, nullptr);
    ASSERT_EQ(r->children.size(), 1u);
    auto* diff = dynamic_cast<ModularCall*>(r->children[0].get());
    ASSERT_NE(diff, nullptr);
    EXPECT_EQ(diff->name->name, "difference");
    EXPECT_EQ(diff->children.size(), 2u);
}

TEST(RenderExpression, SupportsMemberAccess) {
    // Lives in `primary`, not `expr`, precisely so the existing
    // `postfix "." NAME` rule applies.
    auto ast = parseSrc("v = render() { cube(1); }.volume;");
    ASSERT_EQ(ast.size(), 1u);
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    auto* member = dynamic_cast<PrimaryMember*>(a->expr.get());
    ASSERT_NE(member, nullptr);
    EXPECT_EQ(member->member->name, "volume");
    EXPECT_EQ(member->left->kind(), NodeKind::RenderExpression);
}

TEST(RenderExpression, TakesArguments) {
    auto ast = parseSrc("obj = render($fn=32) { sphere(10); };");
    const RenderExpression* r = renderExprOf(ast);
    ASSERT_NE(r, nullptr);
    ASSERT_EQ(r->arguments.size(), 1u);
    auto* named = dynamic_cast<NamedArgument*>(r->arguments[0].get());
    ASSERT_NE(named, nullptr);
    EXPECT_EQ(named->name->name, "$fn");
}

TEST(RenderExpression, AcceptsEmptyAndMultipleChildren) {
    EXPECT_EQ(renderExprOf(parseSrc("e = render() { };"))->children.size(), 0u);
    EXPECT_EQ(renderExprOf(parseSrc("m = render() { cube(1); sphere(2); cylinder(3); };"))->children.size(), 3u);
}

TEST(RenderExpression, WorksAsArgumentAndListElement) {
    EXPECT_NO_THROW(parseSrc("echo(render() { cube(1); });"));
    EXPECT_NO_THROW(parseSrc("v = [render() { cube(1); }, render() { sphere(2); }];"));
    EXPECT_NO_THROW(parseSrc("v = 1 + render() { cube(1); }.volume;"));
    EXPECT_NO_THROW(parseSrc("function f(a) = render() { cube(a); }.volume;"));
    EXPECT_NO_THROW(parseSrc("v = [for (i = [0:2]) render() { cube(i); }];"));
}

// -- The statement form is untouched --------------------------------------

TEST(RenderExpression, StatementFormIsStillAModularCall) {
    auto ast = parseSrc("render() cube(1);");
    ASSERT_EQ(ast.size(), 1u);
    auto* call = dynamic_cast<ModularCall*>(ast[0].get());
    ASSERT_NE(call, nullptr);
    EXPECT_EQ(call->kind(), NodeKind::ModularCall);
    EXPECT_EQ(call->name->name, "render");
    EXPECT_EQ(call->children.size(), 1u);
}

TEST(RenderExpression, StatementFormKeepsArgumentsAndModifiers) {
    auto ast = parseSrc("render(convexity=4) cube(1);\n#render() sphere(2);\nrender() { cube(1); sphere(2); }");
    ASSERT_EQ(ast.size(), 3u);
    auto* withArgs = dynamic_cast<ModularCall*>(ast[0].get());
    ASSERT_NE(withArgs, nullptr);
    EXPECT_EQ(withArgs->arguments.size(), 1u);
    EXPECT_EQ(ast[1]->kind(), NodeKind::ModularModifierHighlight);
    auto* braced = dynamic_cast<ModularCall*>(ast[2].get());
    ASSERT_NE(braced, nullptr);
    EXPECT_EQ(braced->children.size(), 2u);
}

// -- The sharp edge, asserted on purpose ----------------------------------

TEST(RenderExpression, BareCallChildDoesNotTerminateTheAssignment) {
    // `cube(1);` IS the child_statement, semicolon included, so the enclosing
    // assignment is left unterminated. Inherent to OpenSCAD's grammar, not a
    // choice this implementation makes. The braced form is the documented
    // idiom; this test exists so the behaviour is pinned rather than
    // rediscovered.
    EXPECT_THROW(parseSrc("obj = render() cube(1);"), ParseError);
    EXPECT_NO_THROW(parseSrc("obj = render() { cube(1); };"));
}

// -- Reserving the keyword has costs; pin them ----------------------------

TEST(RenderExpression, RenderIsNowAReservedWord) {
    EXPECT_THROW(parseSrc("render = 1;"), ParseError);
    EXPECT_THROW(parseSrc("module render() { cube(1); }"), ParseError);
    EXPECT_THROW(parseSrc("function render() = 1;"), ParseError);
    EXPECT_THROW(parseSrc("cube(render=1);"), ParseError);
    EXPECT_THROW(parseSrc("x = obj.render;"), ParseError);
}

TEST(RenderExpression, DollarRenderIsStillAnOrdinaryName) {
    // keywordTable() is keyed on the full IDENT text, `$` included.
    EXPECT_NO_THROW(parseSrc("$render = 1;"));
}

// -- Round-trips ----------------------------------------------------------

TEST(RenderExpression, PrettyPrintOutputReparses) {
    // The whole point: emitted source must parse back. toString()'s own
    // child rendering follows the reference's terminator-free format, so
    // fmtExpr has a dedicated arm that uses fmtBlock instead -- if that arm
    // regresses, the second parse below throws.
    const std::string src =
        "obj = render() difference() { cube(100); sphere(20); };\n"
        "v = render() { cube(1); }.volume;\n"
        "w = render($fn = 32) { x = 5; cube(x); };\n"
        "e = render() { };\n"
        "render() cube(9);\n";
    std::string once = toOpenscad(parseSrc(src));
    std::string twice;
    ASSERT_NO_THROW(twice = toOpenscad(parseSrc(once)));
    EXPECT_EQ(once, twice) << "formatting is not idempotent:\n" << once;
}

TEST(RenderExpression, ToStringIsReparseable) {
    auto ast = parseSrc("obj = render($fn = 32) { cube(1); sphere(2); };");
    const RenderExpression* r = renderExprOf(ast);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->toString(), "render($fn = 32) { cube(1); sphere(2); }");
    EXPECT_NO_THROW(parseSrc("y = " + r->toString() + ";"));
}

TEST(RenderExpression, JsonRoundTrip) {
    const std::string src =
        "obj = render() difference() { cube(100); sphere(20); };\n"
        "w = render($fn = 32) { x = 5; cube(x); };\n"
        "e = render() { };\n";
    auto ast = parseSrc(src);
    std::string before = toOpenscad(ast);
    auto rebuilt = astFromJsonString(astToJsonString(ast));
    EXPECT_EQ(before, toOpenscad(rebuilt));
}

// -- Scope ----------------------------------------------------------------

TEST(RenderExpression, HoistsAssignmentsInItsChildBlock) {
    // buildScope() must call collectHoistedDeclarations, exactly as
    // ModularCall does, or `x` below resolves to nothing.
    EXPECT_NO_THROW(parseSrc("obj = render() { cube(x); x = 5; };"));
}
