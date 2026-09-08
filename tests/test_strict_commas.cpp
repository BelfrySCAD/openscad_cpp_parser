// StrictCommaScope: a trailing comma in a call argument list is an error,
// matching OpenSCAD 2021.01 (BelfrySCAD issue #362).
//
// The scope is deliberately narrow. Measured against 2021.01 itself, it
// rejected a trailing comma in CALL ARGUMENTS and accepted one everywhere
// else -- list literals, list comprehensions, and module/function parameter
// declarations all take one, in 2021.01 as in every version since. Rejecting
// those would fail files 2021.01 loads happily, so they are left alone and
// this file says so case by case.
#include "test_helpers.hpp"

#include <gtest/gtest.h>

using namespace oscad;

namespace {

// Two productions carry it: `arguments` (every call form) and
// `assignments_expr` (let/for/intersection_for). `let` is NOT a call in this
// grammar -- it has its own assignment list -- which the first draft of this
// feature got wrong and this test caught.
const char* const kRejected[] = {
    "cube(1,);",                            // module instantiation
    "translate([0,0,0],) cube(1);",         // ...with a child
    "function g(x) = x; y = g(1,);",        // user function call
    "y = max(1, 2,);",                      // builtin function call
    "y = str(\"a\", \"b\",);",
    "echo(1, 2,);",
    "assert(true, \"m\",);",
    // assignments_expr, not arguments
    "y = let(x = 1, y = 2,) x + y;",
    "let(x = 1,) cube(x);",
    "for (i = [0:2],) cube(i);",
    "intersection_for (i = [0:1],) cube(1);",
    "a = [for (i = [0:1],) i];",
};

// Accepted by 2021.01, so accepted here even under the scope.
const char* const kAccepted[] = {
    "a = [2, 4,];",                         // list literal
    "a = [for (i = [0:2]) i,];",            // list comprehension
    "a = [each [1, 2],];",
    "module m(a, b,) {} m(1, 2);",          // module parameter declaration
    "function f(a, b,) = a + b;",           // function parameter declaration
    "cube(1);",                             // no trailing comma at all
    "y = max(1, 2);",
};

} // namespace

TEST(StrictCommas, CallArgumentTrailingCommaIsAnErrorInScope) {
    StrictCommaScope strict;
    for (const char* src : kRejected) {
        EXPECT_THROW(parseSrc(src), ParseError) << src;
    }
}

TEST(StrictCommas, EverythingElseKeepsItsTrailingComma) {
    StrictCommaScope strict;
    for (const char* src : kAccepted) {
        EXPECT_NO_THROW(parseSrc(src)) << src;
    }
}

TEST(StrictCommas, OffByDefault) {
    // The whole set parses without the scope -- this is opt-in, and every
    // existing caller must be unaffected.
    for (const char* src : kRejected) {
        EXPECT_NO_THROW(parseSrc(src)) << src;
    }
}

TEST(StrictCommas, ScopeNestsAndRestores) {
    EXPECT_FALSE(strictCommasEnabled());
    {
        StrictCommaScope outer;
        EXPECT_TRUE(strictCommasEnabled());
        {
            StrictCommaScope inner;
            EXPECT_TRUE(strictCommasEnabled());
        }
        EXPECT_TRUE(strictCommasEnabled()) << "an inner scope must not switch it off";
    }
    EXPECT_FALSE(strictCommasEnabled());
}

TEST(StrictCommas, RestoresWhenAParseThrows) {
    try {
        StrictCommaScope strict;
        parseSrc("cube(1,);");
        FAIL() << "expected ParseError";
    } catch (const ParseError&) {
    }
    EXPECT_FALSE(strictCommasEnabled()) << "the scope must unwind with the exception";
}

TEST(StrictCommas, TheErrorPointsAtTheComma) {
    StrictCommaScope strict;
    try {
        parseSrc("cube(1,);");
        FAIL() << "expected ParseError";
    } catch (const ParseError& e) {
        const std::string what = e.what();
        EXPECT_NE(what.find("trailing comma in argument list"), std::string::npos) << what;
    }
}

TEST(StrictCommas, AnAssignmentListSaysSo) {
    StrictCommaScope strict;
    try {
        parseSrc("y = let(x = 1,) x;");
        FAIL() << "expected ParseError";
    } catch (const ParseError& e) {
        const std::string what = e.what();
        EXPECT_NE(what.find("trailing comma in assignment list"), std::string::npos) << what;
    }
}
