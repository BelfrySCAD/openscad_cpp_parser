#include "openscad_cpp_parser/api.hpp"
#include "openscad_cpp_parser/pretty_print.hpp"

#include <gtest/gtest.h>

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
