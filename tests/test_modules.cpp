// Ported from openscad_lalr_parser/tests/test_modules.py.
#include "test_helpers.hpp"

#include <gtest/gtest.h>

using namespace oscad;

// -- Module declarations --------------------------------------------

TEST(ModuleDeclarationTest, Simple) {
    auto ast = parseSrc("module foo() { cube(1); }");
    auto* m = dynamic_cast<ModuleDeclaration*>(ast[0].get());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->name->name, "foo");
    EXPECT_EQ(m->parameters.size(), 0u);
    EXPECT_EQ(m->children.size(), 1u);
}

TEST(ModuleDeclarationTest, WithParams) {
    auto ast = parseSrc("module box(size, center=true) { cube(size, center=center); }");
    auto* m = dynamic_cast<ModuleDeclaration*>(ast[0].get());
    ASSERT_NE(m, nullptr);
    ASSERT_EQ(m->parameters.size(), 2u);
    EXPECT_EQ(m->parameters[0]->name->name, "size");
    EXPECT_EQ(m->parameters[1]->name->name, "center");
    EXPECT_NE(m->parameters[1]->defaultValue, nullptr);
}

TEST(ModuleDeclarationTest, EmptyBody) {
    auto ast = parseSrc("module empty() { }");
    auto* m = dynamic_cast<ModuleDeclaration*>(ast[0].get());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->children.size(), 0u);
}

TEST(ModuleDeclarationTest, SingleStatementBody) {
    auto ast = parseSrc("module foo() cube(1);");
    EXPECT_NE(dynamic_cast<ModuleDeclaration*>(ast[0].get()), nullptr);
}

TEST(ModuleDeclarationTest, Str) {
    auto ast = parseSrc("module foo(x) { cube(x); }");
    std::string s = ast[0]->toString();
    EXPECT_NE(s.find("module"), std::string::npos);
    EXPECT_NE(s.find("foo"), std::string::npos);
}

TEST(ModuleDeclarationTest, MultipleParametersTrailingComma) {
    auto ast = parseSrc("module test(x, y,) {}");
    auto* m = dynamic_cast<ModuleDeclaration*>(ast[0].get());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->parameters.size(), 2u);
}

TEST(ModuleDeclarationTest, NamedParameters) {
    auto ast = parseSrc("module test(x=1, y=2) {}");
    auto* m = dynamic_cast<ModuleDeclaration*>(ast[0].get());
    ASSERT_NE(m, nullptr);
    ASSERT_EQ(m->parameters.size(), 2u);
    EXPECT_NE(m->parameters[0]->defaultValue, nullptr);
    EXPECT_NE(m->parameters[1]->defaultValue, nullptr);
}

TEST(ModuleDeclarationTest, WithBody) {
    auto ast = parseSrc("module test() { cube(10); }");
    auto* m = dynamic_cast<ModuleDeclaration*>(ast[0].get());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->children.size(), 1u);
}

TEST(ModuleDeclarationTest, MultipleStatements) {
    auto ast = parseSrc("module test() { cube(10); sphere(5); }");
    auto* m = dynamic_cast<ModuleDeclaration*>(ast[0].get());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->children.size(), 2u);
}

TEST(ModuleDeclarationTest, MixedStatements) {
    auto ast = parseSrc("module test(a, b=2) { s = 3 * a + b; cube(s); }");
    auto* m = dynamic_cast<ModuleDeclaration*>(ast[0].get());
    ASSERT_NE(m, nullptr);
    ASSERT_EQ(m->children.size(), 2u);
    EXPECT_NE(dynamic_cast<Assignment*>(m->children[0].get()), nullptr);
    EXPECT_NE(dynamic_cast<ModularCall*>(m->children[1].get()), nullptr);
}

TEST(ModuleDeclarationTest, Nested) {
    auto ast = parseSrc("module outer() { module inner() {} }");
    auto* outer = dynamic_cast<ModuleDeclaration*>(ast[0].get());
    ASSERT_NE(outer, nullptr);
    EXPECT_EQ(outer->name->name, "outer");
    ASSERT_EQ(outer->children.size(), 1u);
    auto* inner = dynamic_cast<ModuleDeclaration*>(outer->children[0].get());
    ASSERT_NE(inner, nullptr);
    EXPECT_EQ(inner->name->name, "inner");
}

TEST(ModuleDeclarationTest, WithComment) {
    std::string code = "module test() {\nsphere(5);\n// comment\ncube(10);}";
    EXPECT_NO_THROW(parseSrc(code));
    auto ast = parseSrc(code);
    EXPECT_NE(dynamic_cast<ModuleDeclaration*>(ast[0].get()), nullptr);
    EXPECT_NO_THROW(getASTFromString(code, /*includeComments=*/true));
}

// -- Modular calls ------------------------------------------------------

TEST(ModularCallTest, SimpleCall) {
    auto ast = parseSrc("cube(10);");
    auto* c = dynamic_cast<ModularCall*>(ast[0].get());
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->name->name, "cube");
}

TEST(ModularCallTest, WithPositionalArgs) {
    auto ast = parseSrc("translate([1, 2, 3]) cube(1);");
    auto* c = dynamic_cast<ModularCall*>(ast[0].get());
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->name->name, "translate");
    EXPECT_EQ(c->children.size(), 1u);
}

TEST(ModularCallTest, WithNamedArgs) {
    auto ast = parseSrc("cube(size=10, center=true);");
    auto* c = dynamic_cast<ModularCall*>(ast[0].get());
    ASSERT_NE(c, nullptr);
    ASSERT_EQ(c->arguments.size(), 2u);
    EXPECT_NE(dynamic_cast<NamedArgument*>(c->arguments[0].get()), nullptr);
    EXPECT_NE(dynamic_cast<NamedArgument*>(c->arguments[1].get()), nullptr);
}

TEST(ModularCallTest, WithMixedArgs) {
    auto ast = parseSrc("foo(1, b=2);");
    auto* c = dynamic_cast<ModularCall*>(ast[0].get());
    ASSERT_NE(c, nullptr);
    ASSERT_EQ(c->arguments.size(), 2u);
    EXPECT_NE(dynamic_cast<PositionalArgument*>(c->arguments[0].get()), nullptr);
    EXPECT_NE(dynamic_cast<NamedArgument*>(c->arguments[1].get()), nullptr);
}

TEST(ModularCallTest, NestedCalls) {
    auto ast = parseSrc("translate([1,0,0]) rotate([0,0,45]) cube(1);");
    auto* outer = dynamic_cast<ModularCall*>(ast[0].get());
    ASSERT_NE(outer, nullptr);
    EXPECT_EQ(outer->name->name, "translate");
    ASSERT_EQ(outer->children.size(), 1u);
    auto* inner = dynamic_cast<ModularCall*>(outer->children[0].get());
    ASSERT_NE(inner, nullptr);
    EXPECT_EQ(inner->name->name, "rotate");
}

// Not from the Python suite (which never exercises str() with non-empty
// children on ModularCall) -- regression test for a deliberate C++ deviation
// from nodes.py, see the comment on ModularCall::toString() in
// module_instantiation.cpp.
TEST(ModularCallTest, StrRendersChildren) {
    auto ast = parseSrc("translate([1,0,0]) cube(1);");
    EXPECT_EQ(ast[0]->toString(), "translate([1, 0, 0]) cube(1)");
}

TEST(ModularCallTest, WithBlockChildren) {
    auto ast = parseSrc("union() { cube(1); sphere(2); }");
    auto* c = dynamic_cast<ModularCall*>(ast[0].get());
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->children.size(), 2u);
}

TEST(ModularCallTest, EmptyStatement) {
    auto ast = parseSrc("cube(1);");
    auto* c = dynamic_cast<ModularCall*>(ast[0].get());
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->children.size(), 0u);
}

TEST(ModularCallTest, SingleArg) {
    auto ast = parseSrc("cube(10);");
    auto* c = dynamic_cast<ModularCall*>(ast[0].get());
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->arguments.size(), 1u);
}

TEST(ModularCallTest, VectorArgIsSingleArgument) {
    auto ast = parseSrc("cube([10, 20, 30]);");
    auto* c = dynamic_cast<ModularCall*>(ast[0].get());
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->arguments.size(), 1u);
}

// -- Modifiers ------------------------------------------------------

TEST(ModifiersTest, ShowOnly) {
    auto ast = parseSrc("!cube(1);");
    auto* m = dynamic_cast<ModularModifierShowOnly*>(ast[0].get());
    ASSERT_NE(m, nullptr);
    EXPECT_NE(dynamic_cast<ModularCall*>(m->child.get()), nullptr);
}
TEST(ModifiersTest, Highlight) {
    EXPECT_NE(dynamic_cast<ModularModifierHighlight*>(parseSrc("#cube(1);")[0].get()), nullptr);
}
TEST(ModifiersTest, Background) {
    EXPECT_NE(dynamic_cast<ModularModifierBackground*>(parseSrc("%cube(1);")[0].get()), nullptr);
}
TEST(ModifiersTest, Disable) {
    EXPECT_NE(dynamic_cast<ModularModifierDisable*>(parseSrc("*cube(1);")[0].get()), nullptr);
}
TEST(ModifiersTest, Nested) {
    auto ast = parseSrc("!#cube(1);");
    auto* outer = dynamic_cast<ModularModifierShowOnly*>(ast[0].get());
    ASSERT_NE(outer, nullptr);
    EXPECT_NE(dynamic_cast<ModularModifierHighlight*>(outer->child.get()), nullptr);
}
TEST(ModifiersTest, Str) {
    auto ast = parseSrc("!cube(1);");
    EXPECT_EQ(ast[0]->toString(), "!cube(1)");
}
TEST(ModifiersTest, WithTransform) {
    auto ast = parseSrc("!translate([1, 2, 3]) cube(10);");
    auto* m = dynamic_cast<ModularModifierShowOnly*>(ast[0].get());
    ASSERT_NE(m, nullptr);
    auto* call = dynamic_cast<ModularCall*>(m->child.get());
    ASSERT_NE(call, nullptr);
    EXPECT_EQ(call->name->name, "translate");
}

// -- Complex module bodies ------------------------------------------------

TEST(ModuleComplexTest, WithVariables) {
    auto ast = parseSrc("module test() { x = 10; cube(x); }");
    auto* m = dynamic_cast<ModuleDeclaration*>(ast[0].get());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->children.size(), 2u);
}
TEST(ModuleComplexTest, WithConditionals) {
    auto ast = parseSrc("module test() { if (true) cube(10); }");
    auto* m = dynamic_cast<ModuleDeclaration*>(ast[0].get());
    ASSERT_NE(m, nullptr);
    ASSERT_EQ(m->children.size(), 1u);
    EXPECT_NE(dynamic_cast<ModularIf*>(m->children[0].get()), nullptr);
}
TEST(ModuleComplexTest, WithLoops) {
    auto ast = parseSrc("module test() { for (i = [0:5]) translate([i, 0, 0]) cube(1); }");
    auto* m = dynamic_cast<ModuleDeclaration*>(ast[0].get());
    ASSERT_NE(m, nullptr);
    ASSERT_EQ(m->children.size(), 1u);
    EXPECT_NE(dynamic_cast<ModularFor*>(m->children[0].get()), nullptr);
}
TEST(ModuleComplexTest, InstantiationInExpression) {
    EXPECT_NO_THROW(parseSrc("x = cube(10);"));
}

// -- child_statement block-children regression tests ---------------------

TEST(ChildStatementMultipleChildren, ModularCallBlockAllChildrenReturned) {
    auto ast = parseSrc("\ntranslate([1, 2, 3])\n    rotate([4, 5, 6]) {\n        cube([7, 7, 7]);\n        cube([8, 8, "
                         "8]);\n        cube([9, 9, 9]);\n    }\n");
    auto* translate = dynamic_cast<ModularCall*>(ast[0].get());
    ASSERT_NE(translate, nullptr);
    EXPECT_EQ(translate->name->name, "translate");
    ASSERT_EQ(translate->children.size(), 1u);
    auto* rotate = dynamic_cast<ModularCall*>(translate->children[0].get());
    ASSERT_NE(rotate, nullptr);
    EXPECT_EQ(rotate->name->name, "rotate");
    ASSERT_EQ(rotate->children.size(), 3u);
    for (auto& c : rotate->children) {
        auto* call = dynamic_cast<ModularCall*>(c.get());
        ASSERT_NE(call, nullptr);
        EXPECT_EQ(call->name->name, "cube");
    }
}

TEST(ChildStatementMultipleChildren, ForBlockAllChildrenReturned) {
    auto ast = parseSrc("for (i = [0:2]) { cube(i); sphere(i); }");
    auto* forNode = dynamic_cast<ModularFor*>(ast[0].get());
    ASSERT_NE(forNode, nullptr);
    ASSERT_EQ(forNode->body.size(), 2u);
    EXPECT_EQ(dynamic_cast<ModularCall*>(forNode->body[0].get())->name->name, "cube");
    EXPECT_EQ(dynamic_cast<ModularCall*>(forNode->body[1].get())->name->name, "sphere");
}

TEST(ChildStatementMultipleChildren, IfBlockAllChildrenReturned) {
    auto ast = parseSrc("if (true) { cube(1); sphere(2); cylinder(3); }");
    auto* ifNode = dynamic_cast<ModularIf*>(ast[0].get());
    ASSERT_NE(ifNode, nullptr);
    EXPECT_EQ(ifNode->trueBranch.size(), 3u);
}

TEST(ChildStatementMultipleChildren, IfElseBlockBothBranchesComplete) {
    auto ast = parseSrc("if (true) { cube(1); sphere(2); } else { cylinder(3); cube(4); }");
    auto* ifNode = dynamic_cast<ModularIfElse*>(ast[0].get());
    ASSERT_NE(ifNode, nullptr);
    EXPECT_EQ(ifNode->trueBranch.size(), 2u);
    EXPECT_EQ(ifNode->falseBranch.size(), 2u);
}

TEST(ChildStatementMultipleChildren, SingleChildStatementStillWorks) {
    auto ast = parseSrc("translate([1, 0, 0]) cube(5);");
    auto* translate = dynamic_cast<ModularCall*>(ast[0].get());
    ASSERT_NE(translate, nullptr);
    ASSERT_EQ(translate->children.size(), 1u);
    EXPECT_EQ(dynamic_cast<ModularCall*>(translate->children[0].get())->name->name, "cube");
}

// -- Module declaration detailed ------------------------------------------

TEST(ModuleDeclarationDetailed, NoParams) {
    auto ast = parseSrc("module foo() {}");
    auto* m = dynamic_cast<ModuleDeclaration*>(ast[0].get());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->parameters.size(), 0u);
    EXPECT_EQ(m->children.size(), 0u);
}
TEST(ModuleDeclarationDetailed, OneParamNoDefault) {
    auto ast = parseSrc("module foo(x) { cube(x); }");
    auto* m = dynamic_cast<ModuleDeclaration*>(ast[0].get());
    ASSERT_NE(m, nullptr);
    ASSERT_EQ(m->parameters.size(), 1u);
    EXPECT_EQ(m->parameters[0]->name->name, "x");
    EXPECT_EQ(m->parameters[0]->defaultValue, nullptr);
}
TEST(ModuleDeclarationDetailed, OneParamWithDefault) {
    auto ast = parseSrc("module foo(x=10) { cube(x); }");
    auto* m = dynamic_cast<ModuleDeclaration*>(ast[0].get());
    ASSERT_NE(m, nullptr);
    auto* n = dynamic_cast<NumberLiteral*>(m->parameters[0]->defaultValue.get());
    ASSERT_NE(n, nullptr);
    EXPECT_DOUBLE_EQ(n->val, 10.0);
}
TEST(ModuleDeclarationDetailed, ThreeParamsNoDefaults) {
    auto ast = parseSrc("module foo(x, y, z) { cube([x, y, z]); }");
    auto* m = dynamic_cast<ModuleDeclaration*>(ast[0].get());
    ASSERT_NE(m, nullptr);
    ASSERT_EQ(m->parameters.size(), 3u);
    EXPECT_EQ(m->parameters[0]->name->name, "x");
    EXPECT_EQ(m->parameters[1]->name->name, "y");
    EXPECT_EQ(m->parameters[2]->name->name, "z");
}
TEST(ModuleDeclarationDetailed, ThreeParamsWithDefaults) {
    auto ast = parseSrc("module foo(x=1, y=2, z=3) { cube([x, y, z]); }");
    auto* m = dynamic_cast<ModuleDeclaration*>(ast[0].get());
    ASSERT_NE(m, nullptr);
    ASSERT_EQ(m->parameters.size(), 3u);
    EXPECT_DOUBLE_EQ(dynamic_cast<NumberLiteral*>(m->parameters[0]->defaultValue.get())->val, 1.0);
    EXPECT_DOUBLE_EQ(dynamic_cast<NumberLiteral*>(m->parameters[1]->defaultValue.get())->val, 2.0);
    EXPECT_DOUBLE_EQ(dynamic_cast<NumberLiteral*>(m->parameters[2]->defaultValue.get())->val, 3.0);
}
TEST(ModuleDeclarationDetailed, MixedParams) {
    auto ast = parseSrc("module foo(x, y=2, z) { cube([x, y, z]); }");
    auto* m = dynamic_cast<ModuleDeclaration*>(ast[0].get());
    ASSERT_NE(m, nullptr);
    ASSERT_EQ(m->parameters.size(), 3u);
    EXPECT_EQ(m->parameters[0]->defaultValue, nullptr);
    EXPECT_NE(m->parameters[1]->defaultValue, nullptr);
    EXPECT_EQ(m->parameters[2]->defaultValue, nullptr);
}
TEST(ModuleDeclarationDetailed, MultipleChildren) {
    auto ast = parseSrc("module foo() { cube(10); sphere(5); translate([1,2,3]) cylinder(1, 2); }");
    auto* m = dynamic_cast<ModuleDeclaration*>(ast[0].get());
    ASSERT_NE(m, nullptr);
    ASSERT_EQ(m->children.size(), 3u);
    EXPECT_EQ(dynamic_cast<ModularCall*>(m->children[0].get())->name->name, "cube");
    EXPECT_EQ(dynamic_cast<ModularCall*>(m->children[1].get())->name->name, "sphere");
    auto* translate = dynamic_cast<ModularCall*>(m->children[2].get());
    ASSERT_NE(translate, nullptr);
    EXPECT_EQ(translate->name->name, "translate");
    EXPECT_EQ(translate->children.size(), 1u);
}
TEST(ModuleDeclarationDetailed, NoArgsCall) {
    auto ast = parseSrc("cube();");
    auto* c = dynamic_cast<ModularCall*>(ast[0].get());
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->arguments.size(), 0u);
}
TEST(ModuleDeclarationDetailed, ChainedCalls) {
    auto ast = parseSrc("translate([1, 2, 3]) rotate([0, 0, 45]) cube(10);");
    auto* outer = dynamic_cast<ModularCall*>(ast[0].get());
    ASSERT_NE(outer, nullptr);
    EXPECT_EQ(outer->name->name, "translate");
    ASSERT_EQ(outer->children.size(), 1u);
    auto* mid = dynamic_cast<ModularCall*>(outer->children[0].get());
    ASSERT_NE(mid, nullptr);
    EXPECT_EQ(mid->name->name, "rotate");
    ASSERT_EQ(mid->children.size(), 1u);
    auto* inner = dynamic_cast<ModularCall*>(mid->children[0].get());
    ASSERT_NE(inner, nullptr);
    EXPECT_EQ(inner->name->name, "cube");
}
