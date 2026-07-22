// Ported from openscad_lalr_parser/tests/test_use_include.py.
#include "test_helpers.hpp"

#include <gtest/gtest.h>

using namespace oscad;

TEST(UseStatement, Use) {
    auto ast = parseSrc("use <library.scad>");
    auto* u = dynamic_cast<UseStatement*>(ast[0].get());
    ASSERT_NE(u, nullptr);
    EXPECT_EQ(u->filepath->val, "library.scad");
}

TEST(UseStatement, UsePath) {
    auto ast = parseSrc("use <BOSL2/std.scad>");
    auto* u = dynamic_cast<UseStatement*>(ast[0].get());
    ASSERT_NE(u, nullptr);
    EXPECT_EQ(u->filepath->val, "BOSL2/std.scad");
}

TEST(UseStatement, UseStr) {
    auto ast = parseSrc("use <library.scad>");
    EXPECT_EQ(ast[0]->toString(), "use <library.scad>");
}

TEST(UseStatement, UseMultiple) {
    auto ast = parseSrc("use <file1.scad>\nuse <file2.scad>");
    ASSERT_EQ(ast.size(), 2u);
    auto* u0 = dynamic_cast<UseStatement*>(ast[0].get());
    auto* u1 = dynamic_cast<UseStatement*>(ast[1].get());
    ASSERT_NE(u0, nullptr);
    ASSERT_NE(u1, nullptr);
    EXPECT_EQ(u0->filepath->val, "file1.scad");
    EXPECT_EQ(u1->filepath->val, "file2.scad");
}

TEST(UseStatement, UseWithCode) {
    auto ast = parseSrc("use <file.scad>\nx = 1;");
    EXPECT_NE(dynamic_cast<UseStatement*>(ast[0].get()), nullptr);
    EXPECT_NE(dynamic_cast<Assignment*>(ast[1].get()), nullptr);
}

TEST(IncludeStatement, Include) {
    auto ast = parseSrc("include <config.scad>");
    auto* i = dynamic_cast<IncludeStatement*>(ast[0].get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->filepath->val, "config.scad");
}

TEST(IncludeStatement, IncludePath) {
    auto ast = parseSrc("include <utils/math.scad>");
    auto* i = dynamic_cast<IncludeStatement*>(ast[0].get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->filepath->val, "utils/math.scad");
}

TEST(IncludeStatement, IncludeStr) {
    auto ast = parseSrc("include <config.scad>");
    EXPECT_EQ(ast[0]->toString(), "include <config.scad>");
}

TEST(IncludeStatement, IncludeMultiple) {
    auto ast = parseSrc("include <file1.scad>\ninclude <file2.scad>");
    ASSERT_EQ(ast.size(), 2u);
    EXPECT_EQ(dynamic_cast<IncludeStatement*>(ast[0].get())->filepath->val, "file1.scad");
    EXPECT_EQ(dynamic_cast<IncludeStatement*>(ast[1].get())->filepath->val, "file2.scad");
}

TEST(IncludeStatement, IncludeWithCode) {
    auto ast = parseSrc("include <file.scad>\nx = 1;");
    EXPECT_NE(dynamic_cast<IncludeStatement*>(ast[0].get()), nullptr);
    EXPECT_NE(dynamic_cast<Assignment*>(ast[1].get()), nullptr);
}

TEST(MixedUseInclude, UseAndInclude) {
    auto ast = parseSrc("use <lib.scad>\ninclude <config.scad>\ncube(1);");
    ASSERT_EQ(ast.size(), 3u);
    EXPECT_NE(dynamic_cast<UseStatement*>(ast[0].get()), nullptr);
    EXPECT_NE(dynamic_cast<IncludeStatement*>(ast[1].get()), nullptr);
}

TEST(MixedUseInclude, UseThenInclude) {
    auto ast = parseSrc("use <file1.scad>\ninclude <file2.scad>");
    EXPECT_NE(dynamic_cast<UseStatement*>(ast[0].get()), nullptr);
    EXPECT_NE(dynamic_cast<IncludeStatement*>(ast[1].get()), nullptr);
}

TEST(MixedUseInclude, IncludeThenUse) {
    auto ast = parseSrc("include <file1.scad>\nuse <file2.scad>");
    EXPECT_NE(dynamic_cast<IncludeStatement*>(ast[0].get()), nullptr);
    EXPECT_NE(dynamic_cast<UseStatement*>(ast[1].get()), nullptr);
}

TEST(MixedUseInclude, MultipleMixed) {
    auto ast = parseSrc("use <file1.scad>\ninclude <file2.scad>\nuse <file3.scad>");
    ASSERT_EQ(ast.size(), 3u);
    EXPECT_NE(dynamic_cast<UseStatement*>(ast[0].get()), nullptr);
    EXPECT_NE(dynamic_cast<IncludeStatement*>(ast[1].get()), nullptr);
    EXPECT_NE(dynamic_cast<UseStatement*>(ast[2].get()), nullptr);
}
