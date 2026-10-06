// `profile_time()`: the statement form times its children, the expression
// form (`x = profile_time("label") expr;`) its body. `profile_time` is a
// reserved keyword only because the expression form would otherwise be
// indistinguishable from a function call; the statement form is a plain
// ModularCall, so builtin dispatch downstream sees an ordinary module.

#include "openscad_cpp_parser/api.hpp"
#include "openscad_cpp_parser/pretty_print.hpp"
#include "openscad_cpp_parser/serialization.hpp"
#include "test_helpers.hpp"

#include <gtest/gtest.h>

using namespace oscad;

namespace {

const ProfileTimeOp* profileOf(const std::vector<std::unique_ptr<ASTNode>>& ast) {
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    return a ? dynamic_cast<const ProfileTimeOp*>(a->expr.get()) : nullptr;
}

} // namespace

TEST(ProfileTime, StatementFormIsAPlainModularCall) {
    auto ast = parseSrc("profile_time(\"gears\") { cube(1); sphere(2); }");
    ASSERT_EQ(ast.size(), 1u);
    auto* call = dynamic_cast<ModularCall*>(ast[0].get());
    ASSERT_NE(call, nullptr);
    EXPECT_EQ(call->name->name, "profile_time");
    EXPECT_EQ(call->arguments.size(), 1u);
    EXPECT_EQ(call->children.size(), 2u);
}

TEST(ProfileTime, ExpressionFormWrapsItsBody) {
    auto ast = parseSrc("x = profile_time(\"sum\") 1 + 2;");
    const ProfileTimeOp* p = profileOf(ast);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->kind(), NodeKind::ProfileTimeOp);
    EXPECT_EQ(p->arguments.size(), 1u);
    EXPECT_EQ(p->body->kind(), NodeKind::AdditionOp);
}

TEST(ProfileTime, ExpressionFormAllowsNoLabel) {
    const ProfileTimeOp* p = nullptr;
    auto ast = parseSrc("x = profile_time() f(3);");
    p = profileOf(ast);
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(p->arguments.empty());
}

TEST(ProfileTime, ExpressionFormNeedsABody) {
    EXPECT_THROW(parseSrc("x = profile_time(\"a\");"), ParseError);
}

TEST(ProfileTime, IsReserved) {
    EXPECT_THROW(parseSrc("profile_time = 1;"), ParseError);
    EXPECT_THROW(parseSrc("function profile_time() = 1;"), ParseError);
    EXPECT_THROW(parseSrc("module profile_time() {}"), ParseError);
    // `$profile_time` is a different identifier and stays usable.
    EXPECT_NO_THROW(parseSrc("$profile_time = 1;"));
}

TEST(ProfileTime, ToStringRoundTrips) {
    auto ast = parseSrc("x = profile_time(\"a\") 1 + 2;");
    EXPECT_EQ(ast[0]->toString(), "x = profile_time(\"a\") 1 + 2");
    EXPECT_NE(profileOf(parseSrc(ast[0]->toString() + ";")), nullptr);
}

TEST(ProfileTime, JsonRoundTrips) {
    auto ast = parseSrc("x = profile_time(\"a\") 1 + 2;");
    auto back = astFromJsonString(astToJsonString(ast));
    const ProfileTimeOp* p = profileOf(back);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->body->kind(), NodeKind::AdditionOp);
}

TEST(ProfileTime, PrettyPrintReparses) {
    for (const char* src : {"x = profile_time(\"a\") 1 + 2;",
                            "profile_time(\"g\") { cube(1); }"}) {
        auto ast = parseSrc(src);
        std::string out = toOpenscad(ast);
        EXPECT_NE(out.find("profile_time(\""), std::string::npos) << out;
        EXPECT_NO_THROW(parseSrc(out)) << out;
    }
}
