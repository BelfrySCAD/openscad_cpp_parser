// Ported from openscad_lalr_parser/tests/test_scope.py.
//
// Note: Scope::__repr__ (Python's debug repr, e.g. "vars=[x] funcs=[none]
// mods=[none]") has no C++ equivalent -- it's a pure debug-string
// convenience with no behavioral content, so test_repr_empty/
// test_repr_with_bindings aren't ported. Everything else is.
#include "test_helpers.hpp"

#include <gtest/gtest.h>

using namespace oscad;

// -- Scope basics (Scope class in isolation) -------------------------

TEST(ScopeBasics, EmptyScope) {
    Scope scope;
    EXPECT_EQ(scope.parent(), nullptr);
    EXPECT_EQ(scope.lookupVariable("x"), nullptr);
    EXPECT_EQ(scope.lookupFunction("f"), nullptr);
    EXPECT_EQ(scope.lookupModule("m"), nullptr);
}

TEST(ScopeBasics, LookupVariableNotFound) {
    Scope scope;
    EXPECT_EQ(scope.lookupVariable("x"), nullptr);
}
TEST(ScopeBasics, LookupFunctionNotFound) {
    Scope scope;
    EXPECT_EQ(scope.lookupFunction("f"), nullptr);
}
TEST(ScopeBasics, LookupModuleNotFound) {
    Scope scope;
    EXPECT_EQ(scope.lookupModule("m"), nullptr);
}

TEST(ScopeBasics, ChildScope) {
    Scope parent;
    Scope& child = parent.childScope();
    EXPECT_EQ(child.parent(), &parent);
    EXPECT_EQ(child.lookupVariable("x"), nullptr);
}

TEST(ScopeBasics, DefineAndLookupVariable) {
    auto ast = parseSrc("x = 1;");
    Scope scope;
    scope.defineVariable("x", ast[0].get());
    EXPECT_EQ(scope.lookupVariable("x"), ast[0].get());
}
TEST(ScopeBasics, DefineAndLookupFunction) {
    auto ast = parseSrc("function f(a) = a;");
    Scope scope;
    scope.defineFunction("f", ast[0].get());
    EXPECT_EQ(scope.lookupFunction("f"), ast[0].get());
}
TEST(ScopeBasics, DefineAndLookupModule) {
    auto ast = parseSrc("module m() { cube(1); }");
    Scope scope;
    scope.defineModule("m", ast[0].get());
    EXPECT_EQ(scope.lookupModule("m"), ast[0].get());
}

TEST(ScopeBasics, LookupVariableInParent) {
    auto ast = parseSrc("x = 1;");
    Scope parent;
    Scope& child = parent.childScope();
    parent.defineVariable("x", ast[0].get());
    EXPECT_EQ(child.lookupVariable("x"), ast[0].get());
}
TEST(ScopeBasics, LookupFunctionInParent) {
    auto ast = parseSrc("function f(a) = a;");
    Scope parent;
    Scope& child = parent.childScope();
    parent.defineFunction("f", ast[0].get());
    EXPECT_EQ(child.lookupFunction("f"), ast[0].get());
}
TEST(ScopeBasics, LookupModuleInParent) {
    auto ast = parseSrc("module m() { cube(1); }");
    Scope parent;
    Scope& child = parent.childScope();
    parent.defineModule("m", ast[0].get());
    EXPECT_EQ(child.lookupModule("m"), ast[0].get());
}

// -- build_scopes basics --------------------------------------------

TEST(ScopeBuilderBasics, EmptyAst) {
    std::vector<std::unique_ptr<ASTNode>> empty;
    auto scope = buildScopes(empty);
    ASSERT_NE(scope, nullptr);
    EXPECT_EQ(scope->lookupVariable("x"), nullptr);
}

TEST(ScopeBuilderBasics, SimpleAssignment) {
    auto ast = parseSrc("x = 42;");
    auto scope = buildScopes(ast);
    EXPECT_NE(scope->lookupVariable("x"), nullptr);
    EXPECT_EQ(scope->lookupVariable("x"), ast[0].get());
}

TEST(ScopeBuilderBasics, AssignmentScopeAttached) {
    auto ast = parseSrc("x = 42;");
    auto scope = buildScopes(ast);
    EXPECT_EQ(ast[0]->scope(), scope.get());
}

TEST(ScopeBuilderBasics, MultipleAssignments) {
    auto ast = parseSrc("x = 1;\ny = 2;\nz = 3;");
    auto scope = buildScopes(ast);
    EXPECT_EQ(scope->lookupVariable("x"), ast[0].get());
    EXPECT_EQ(scope->lookupVariable("y"), ast[1].get());
    EXPECT_EQ(scope->lookupVariable("z"), ast[2].get());
}

// -- Function scope -----------------------------------------------------

TEST(FunctionScopeTest, InRoot) {
    auto ast = parseSrc("function add(a, b) = a + b;");
    auto scope = buildScopes(ast);
    EXPECT_EQ(scope->lookupFunction("add"), ast[0].get());
}

TEST(FunctionScopeTest, ParametersInFunctionScope) {
    auto ast = parseSrc("function add(a, b) = a + b;");
    auto scope = buildScopes(ast);
    auto* func = dynamic_cast<FunctionDeclaration*>(ast[0].get());
    ASSERT_NE(func, nullptr);
    Scope* bodyScope = func->expr->scope();
    ASSERT_NE(bodyScope, nullptr);
    EXPECT_NE(bodyScope->parent(), nullptr);
    EXPECT_NE(bodyScope->lookupVariable("a"), nullptr);
    EXPECT_NE(bodyScope->lookupVariable("b"), nullptr);
}

TEST(FunctionScopeTest, SeesOuterVars) {
    auto ast = parseSrc("x = 10;\nfunction f(a) = a + x;");
    auto scope = buildScopes(ast);
    auto* func = dynamic_cast<FunctionDeclaration*>(ast[1].get());
    ASSERT_NE(func, nullptr);
    EXPECT_NE(func->expr->scope()->lookupVariable("x"), nullptr);
}

TEST(FunctionScopeTest, ParameterWithDefault) {
    auto ast = parseSrc("function f(a=5) = a;");
    auto* func = dynamic_cast<FunctionDeclaration*>(ast[0].get());
    ASSERT_NE(func, nullptr);
    EXPECT_NE(func->parameters[0]->defaultValue, nullptr);
}

TEST(FunctionScopeTest, ParameterDefaultVisitedInCallerScope) {
    // "Parameter defaults are evaluated in the caller scope, not the
    // function body scope" -- the single highest-value scope test.
    auto ast = parseSrc("x = 10;\nfunction f(a=x) = a;");
    auto scope = buildScopes(ast);
    auto* func = dynamic_cast<FunctionDeclaration*>(ast[1].get());
    ASSERT_NE(func, nullptr);
    auto& param = func->parameters[0];
    Scope* defaultScope = param->defaultValue->scope();
    ASSERT_NE(defaultScope, nullptr);
    EXPECT_NE(defaultScope->lookupVariable("x"), nullptr);
    // Strengthened beyond the Python original: confirm the default's scope
    // really is the caller scope, not the function's own body scope (which
    // would additionally see `a`).
    EXPECT_NE(defaultScope, func->expr->scope());
    EXPECT_EQ(defaultScope->lookupVariable("a"), nullptr);
}

// -- Module scope -------------------------------------------------------

TEST(ModuleScopeTest, InRoot) {
    auto ast = parseSrc("module box(size) { cube(size); }");
    auto scope = buildScopes(ast);
    EXPECT_EQ(scope->lookupModule("box"), ast[0].get());
}

TEST(ModuleScopeTest, ParametersInModuleScope) {
    auto ast = parseSrc("module box(size) { cube(size); }");
    auto scope = buildScopes(ast);
    auto* mod = dynamic_cast<ModuleDeclaration*>(ast[0].get());
    ASSERT_NE(mod, nullptr);
    Scope* childScope = mod->children[0]->scope();
    ASSERT_NE(childScope, nullptr);
    EXPECT_NE(childScope->lookupVariable("size"), nullptr);
}

TEST(ModuleScopeTest, ParameterWithDefault) {
    auto ast = parseSrc("module box(size=10) { cube(size); }");
    auto* mod = dynamic_cast<ModuleDeclaration*>(ast[0].get());
    ASSERT_NE(mod, nullptr);
    EXPECT_NE(mod->parameters[0]->defaultValue, nullptr);
}

TEST(ModuleScopeTest, NestedFunctionInModule) {
    auto ast = parseSrc("module m() {\n  function helper(x) = x * 2;\n  cube(helper(5));\n}");
    auto scope = buildScopes(ast);
    auto* mod = dynamic_cast<ModuleDeclaration*>(ast[0].get());
    ASSERT_NE(mod, nullptr);
    Scope* bodyScope = mod->children[0]->scope();
    ASSERT_NE(bodyScope, nullptr);
    EXPECT_NE(bodyScope->lookupFunction("helper"), nullptr);
}

// -- Hoisting -------------------------------------------------------

TEST(HoistingTest, AssignmentHoistedInModule) {
    auto ast = parseSrc("module m() {\n  cube(val);\n  val = 10;\n}");
    auto scope = buildScopes(ast);
    auto* mod = dynamic_cast<ModuleDeclaration*>(ast[0].get());
    ASSERT_NE(mod, nullptr);
    ASTNode* callNode = mod->children[0].get();
    EXPECT_NE(callNode->scope()->lookupVariable("val"), nullptr);
}

// -- Let expressions ------------------------------------------------

TEST(LetExpressionsTest, LetOpCreatesScope) {
    auto ast = parseSrc("x = let(a=1, b=2) a + b;");
    auto scope = buildScopes(ast);
    auto* letNode = dynamic_cast<LetOp*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(letNode, nullptr);
    Scope* bodyScope = letNode->body->scope();
    ASSERT_NE(bodyScope, nullptr);
    EXPECT_NE(bodyScope->lookupVariable("a"), nullptr);
    EXPECT_NE(bodyScope->lookupVariable("b"), nullptr);
}

TEST(LetExpressionsTest, LetVarsNotInOuterScope) {
    auto ast = parseSrc("x = let(a=1) a;");
    auto scope = buildScopes(ast);
    EXPECT_EQ(scope->lookupVariable("a"), nullptr);
    EXPECT_NE(scope->lookupVariable("x"), nullptr);
}

// -- Modular constructs -----------------------------------------------

TEST(ModularConstructsTest, ForCreatesScope) {
    auto ast = parseSrc("for (i=[0:5]) cube(i);");
    auto scope = buildScopes(ast);
    auto* forNode = dynamic_cast<ModularFor*>(ast[0].get());
    ASSERT_NE(forNode, nullptr);
    ASSERT_FALSE(forNode->body.empty());
    EXPECT_NE(forNode->body[0]->scope()->lookupVariable("i"), nullptr);
    EXPECT_EQ(scope->lookupVariable("i"), nullptr);
}

TEST(ModularConstructsTest, IfCreatesScope) {
    auto ast = parseSrc("if (true) cube(1);");
    auto scope = buildScopes(ast);
    auto* ifNode = dynamic_cast<ModularIf*>(ast[0].get());
    ASSERT_NE(ifNode, nullptr);
    ASSERT_FALSE(ifNode->trueBranch.empty());
    EXPECT_EQ(ifNode->trueBranch[0]->scope()->parent(), scope.get());
}

TEST(ModularConstructsTest, LetCreatesScope) {
    auto ast = parseSrc("let (x=5) cube(x);");
    auto scope = buildScopes(ast);
    auto* letNode = dynamic_cast<ModularLet*>(ast[0].get());
    ASSERT_NE(letNode, nullptr);
    ASSERT_FALSE(letNode->children.empty());
    EXPECT_NE(letNode->children[0]->scope()->lookupVariable("x"), nullptr);
}

TEST(ModularConstructsTest, IfSingleBranch) {
    auto ast = parseSrc("if (x) cube(1);");
    auto scope = buildScopes(ast);
    auto* ifNode = dynamic_cast<ModularIf*>(ast[0].get());
    ASSERT_NE(ifNode, nullptr);
    EXPECT_NE(ifNode->trueBranch[0]->scope(), nullptr);
}

TEST(ModularConstructsTest, IfElseSingleBranches) {
    auto ast = parseSrc("if (x) cube(1); else sphere(2);");
    auto scope = buildScopes(ast);
    auto* ieNode = dynamic_cast<ModularIfElse*>(ast[0].get());
    ASSERT_NE(ieNode, nullptr);
    Scope* trueScope = ieNode->trueBranch[0]->scope();
    Scope* falseScope = ieNode->falseBranch[0]->scope();
    EXPECT_NE(trueScope, nullptr);
    EXPECT_NE(falseScope, nullptr);
    EXPECT_NE(trueScope, falseScope);
}

TEST(ModularConstructsTest, ForListBody) {
    auto ast = parseSrc("for (i=[0:3]) { cube(i); sphere(i); }");
    auto scope = buildScopes(ast);
    auto* forNode = dynamic_cast<ModularFor*>(ast[0].get());
    ASSERT_NE(forNode, nullptr);
    for (auto& child : forNode->body) {
        EXPECT_NE(child->scope()->lookupVariable("i"), nullptr);
    }
}

TEST(ModularConstructsTest, EchoWithChildren) {
    auto ast = parseSrc("echo(\"test\") cube(1);");
    auto scope = buildScopes(ast);
    auto* echoNode = dynamic_cast<ModularEcho*>(ast[0].get());
    ASSERT_NE(echoNode, nullptr);
    EXPECT_EQ(echoNode->scope(), scope.get());
    for (auto& child : echoNode->children) {
        EXPECT_NE(child->scope(), nullptr);
    }
}

TEST(ModularConstructsTest, AssertWithChildren) {
    auto ast = parseSrc("assert(x > 0) cube(x);");
    auto scope = buildScopes(ast);
    auto* assertNode = dynamic_cast<ModularAssert*>(ast[0].get());
    ASSERT_NE(assertNode, nullptr);
    EXPECT_EQ(assertNode->scope(), scope.get());
}

TEST(ModularConstructsTest, CallEmptyChildren) {
    auto ast = parseSrc("cube(1);");
    auto scope = buildScopes(ast);
    EXPECT_EQ(ast[0]->scope(), scope.get());
}

TEST(ModularConstructsTest, ModifierShowOnly) {
    auto ast = parseSrc("! cube(1);");
    auto scope = buildScopes(ast);
    auto* mod = dynamic_cast<ModularModifierShowOnly*>(ast[0].get());
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(mod->scope(), scope.get());
    EXPECT_NE(mod->child->scope(), nullptr);
}
TEST(ModularConstructsTest, ModifierHighlight) {
    auto ast = parseSrc("# cube(1);");
    auto scope = buildScopes(ast);
    auto* mod = dynamic_cast<ModularModifierHighlight*>(ast[0].get());
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(mod->scope(), scope.get());
}
TEST(ModularConstructsTest, ModifierBackground) {
    auto ast = parseSrc("% cube(1);");
    auto scope = buildScopes(ast);
    auto* mod = dynamic_cast<ModularModifierBackground*>(ast[0].get());
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(mod->scope(), scope.get());
}
TEST(ModularConstructsTest, ModifierDisable) {
    auto ast = parseSrc("* cube(1);");
    auto scope = buildScopes(ast);
    auto* mod = dynamic_cast<ModularModifierDisable*>(ast[0].get());
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(mod->scope(), scope.get());
}

// -- FunctionLiteral recursion --------------------------------------

TEST(FunctionLiteralRecursionTest, SeesAssignedVariable) {
    auto ast = parseSrc("f = function(x) x + 1;");
    auto scope = buildScopes(ast);
    auto* funcLit = dynamic_cast<FunctionLiteral*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(funcLit, nullptr);
    EXPECT_NE(funcLit->body->scope()->lookupVariable("x"), nullptr);
}

TEST(FunctionLiteralRecursionTest, WithDefaultParameter) {
    auto ast = parseSrc("f = function(x=5) x;");
    auto scope = buildScopes(ast);
    auto* funcLit = dynamic_cast<FunctionLiteral*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(funcLit, nullptr);
    EXPECT_NE(funcLit->parameters[0]->defaultValue, nullptr);
    EXPECT_NE(funcLit->body->scope()->lookupVariable("x"), nullptr);
}

TEST(FunctionLiteralRecursionTest, InExpression) {
    auto ast = parseSrc("x = [1, function(a) a];");
    auto scope = buildScopes(ast);
    auto* lc = dynamic_cast<ListComprehension*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(lc, nullptr);
    auto* funcLit = dynamic_cast<FunctionLiteral*>(lc->elements[1].get());
    ASSERT_NE(funcLit, nullptr);
    EXPECT_NE(funcLit->body->scope()->lookupVariable("a"), nullptr);
}

TEST(FunctionLiteralRecursionTest, InTernaryRhs) {
    auto ast = parseSrc("x = true ? 1 : function(a) a;");
    auto scope = buildScopes(ast);
    auto* ternary = dynamic_cast<TernaryOp*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(ternary, nullptr);
    auto* funcLit = dynamic_cast<FunctionLiteral*>(ternary->falseExpr.get());
    ASSERT_NE(funcLit, nullptr);
    EXPECT_NE(funcLit->body->scope()->lookupVariable("a"), nullptr);
}

// -- ModularCall children -------------------------------------------

TEST(ModularCallChildrenTest, WithNamedArgument) {
    auto ast = parseSrc("cube(size=10);");
    auto scope = buildScopes(ast);
    auto* call = dynamic_cast<ModularCall*>(ast[0].get());
    ASSERT_NE(call, nullptr);
    auto* named = dynamic_cast<NamedArgument*>(call->arguments[0].get());
    ASSERT_NE(named, nullptr);
    EXPECT_EQ(named->scope(), scope.get());
    EXPECT_EQ(named->name->scope(), scope.get());
    EXPECT_EQ(named->expr->scope(), scope.get());
}

TEST(ModularCallChildrenTest, PrimaryCallNamedArgumentVisitsName) {
    auto ast = parseSrc("x = f(a=1);");
    auto scope = buildScopes(ast);
    auto* pcall = dynamic_cast<PrimaryCall*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(pcall, nullptr);
    auto* named = dynamic_cast<NamedArgument*>(pcall->arguments[0].get());
    ASSERT_NE(named, nullptr);
    EXPECT_EQ(named->name->scope(), scope.get());
}

TEST(ModularCallChildrenTest, CallChildrenScope) {
    auto ast = parseSrc("translate([0,0,0]) { cube(1); sphere(2); }");
    auto scope = buildScopes(ast);
    auto* call = dynamic_cast<ModularCall*>(ast[0].get());
    ASSERT_NE(call, nullptr);
    for (auto& child : call->children) {
        ASSERT_NE(child->scope(), nullptr);
        EXPECT_EQ(child->scope()->parent(), scope.get());
    }
}

// -- Scope lookup / shadowing -----------------------------------------

TEST(ScopeLookupTest, AncestorScopeFunctionAndModule) {
    auto ast = parseSrc("x = 1;\nfunction f() = x;\nmodule m() { cube(x); }");
    auto scope = buildScopes(ast);
    auto* func = dynamic_cast<FunctionDeclaration*>(ast[1].get());
    ASSERT_NE(func, nullptr);
    EXPECT_NE(func->expr->scope()->lookupVariable("x"), nullptr);
    auto* mod = dynamic_cast<ModuleDeclaration*>(ast[2].get());
    ASSERT_NE(mod, nullptr);
    EXPECT_NE(mod->children[0]->scope()->lookupVariable("x"), nullptr);
}

TEST(ScopeLookupTest, LookupInParent) {
    auto ast = parseSrc("x = 1;");
    Scope parent;
    Scope& child = parent.childScope();
    parent.defineVariable("x", ast[0].get());
    EXPECT_EQ(child.lookupVariable("x"), ast[0].get());
}

TEST(ScopeLookupTest, Shadowing) {
    auto ast = parseSrc("x = 1;\nfunction f(x) = x;");
    auto scope = buildScopes(ast);
    auto* func = dynamic_cast<FunctionDeclaration*>(ast[1].get());
    ASSERT_NE(func, nullptr);
    Scope* bodyScope = func->expr->scope();
    ASTNode* found = bodyScope->lookupVariable("x");
    ASSERT_NE(found, nullptr);
    EXPECT_NE(dynamic_cast<ParameterDeclaration*>(found), nullptr);
    EXPECT_NE(found, ast[0].get());
}

// -- Three namespaces -------------------------------------------------

TEST(ThreeNamespacesTest, SameNameInAllThree) {
    auto ast = parseSrc("thing = 42;\nfunction thing() = 1;\nmodule thing() { cube(1); }");
    auto scope = buildScopes(ast);
    ASTNode* var = scope->lookupVariable("thing");
    ASTNode* func = scope->lookupFunction("thing");
    ASTNode* mod = scope->lookupModule("thing");
    ASSERT_NE(var, nullptr);
    ASSERT_NE(func, nullptr);
    ASSERT_NE(mod, nullptr);
    EXPECT_NE(dynamic_cast<Assignment*>(var), nullptr);
    EXPECT_NE(dynamic_cast<FunctionDeclaration*>(func), nullptr);
    EXPECT_NE(dynamic_cast<ModuleDeclaration*>(mod), nullptr);
    EXPECT_NE(var, func);
    EXPECT_NE(var, mod);
    EXPECT_NE(func, mod);
}

// -- List comprehension scope -------------------------------------------

TEST(ListComprehensionScopeTest, ForScope) {
    auto ast = parseSrc("x = [for (i=[0:3]) i];");
    auto scope = buildScopes(ast);
    auto* lc = dynamic_cast<ListComprehension*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(lc, nullptr);
    auto* forElem = dynamic_cast<ListCompFor*>(lc->elements[0].get());
    ASSERT_NE(forElem, nullptr);
    EXPECT_NE(forElem->body->scope()->lookupVariable("i"), nullptr);
    EXPECT_EQ(scope->lookupVariable("i"), nullptr);
}

TEST(ListComprehensionScopeTest, CForScope) {
    auto ast = parseSrc("x = [for (i=0; i<5; i=i+1) i];");
    auto scope = buildScopes(ast);
    auto* lc = dynamic_cast<ListComprehension*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(lc, nullptr);
    auto* cforElem = dynamic_cast<ListCompCFor*>(lc->elements[0].get());
    ASSERT_NE(cforElem, nullptr);
    EXPECT_NE(cforElem->body->scope()->lookupVariable("i"), nullptr);
    // Not checked by the Python original, but symmetric with the plain-for
    // case and worth locking down: c-style-for's loop var must not leak.
    EXPECT_EQ(scope->lookupVariable("i"), nullptr);
}

TEST(ListComprehensionScopeTest, LetScope) {
    auto ast = parseSrc("x = [let(a=5) for(i=[0:a]) i];");
    auto scope = buildScopes(ast);
    auto* lc = dynamic_cast<ListComprehension*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(lc, nullptr);
    auto* letElem = dynamic_cast<ListCompLet*>(lc->elements[0].get());
    ASSERT_NE(letElem, nullptr);
    EXPECT_NE(letElem->body->scope()->lookupVariable("a"), nullptr);
}

TEST(ListComprehensionScopeTest, IfScope) {
    auto ast = parseSrc("x = [for (i=[0:5]) if (i > 2) i];");
    auto scope = buildScopes(ast);
    auto* lc = dynamic_cast<ListComprehension*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(lc, nullptr);
    auto* forElem = dynamic_cast<ListCompFor*>(lc->elements[0].get());
    ASSERT_NE(forElem, nullptr);
    auto* ifElem = dynamic_cast<ListCompIf*>(forElem->body.get());
    ASSERT_NE(ifElem, nullptr);
    EXPECT_NE(ifElem->scope(), nullptr);
    EXPECT_NE(ifElem->trueExpr->scope(), nullptr);
}

TEST(ListComprehensionScopeTest, IfElseScope) {
    auto ast = parseSrc("x = [for (i=[0:5]) if (i > 2) i else -i];");
    auto scope = buildScopes(ast);
    auto* lc = dynamic_cast<ListComprehension*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(lc, nullptr);
    auto* forElem = dynamic_cast<ListCompFor*>(lc->elements[0].get());
    ASSERT_NE(forElem, nullptr);
    auto* ifElseElem = dynamic_cast<ListCompIfElse*>(forElem->body.get());
    ASSERT_NE(ifElseElem, nullptr);
    EXPECT_NE(ifElseElem->trueExpr->scope(), nullptr);
    EXPECT_NE(ifElseElem->falseExpr->scope(), nullptr);
    // Unlike ModularIfElse, ListCompIfElse's branches are plain
    // expressions (not statement blocks that could contain hoistable
    // declarations), so build_scope legitimately does NOT create a new
    // scope per branch here -- both share parent_scope directly. Matches
    // the reference exactly; do not "strengthen" this into a distinctness
    // check (an earlier version of this test incorrectly did).
    EXPECT_EQ(ifElseElem->trueExpr->scope(), ifElseElem->falseExpr->scope());
}

TEST(ListComprehensionScopeTest, EachScope) {
    auto ast = parseSrc("x = [each [1, 2, 3]];");
    auto scope = buildScopes(ast);
    auto* lc = dynamic_cast<ListComprehension*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(lc, nullptr);
    auto* eachElem = dynamic_cast<ListCompEach*>(lc->elements[0].get());
    ASSERT_NE(eachElem, nullptr);
    EXPECT_NE(eachElem->scope(), nullptr);
}

// -- Expression-operator build_scope sweep -------------------------------
// Mechanical check: every expression node type gets .scope() populated by
// build_scope(), and simple operators don't introduce a new scope level
// (their scope() is identity-equal to the enclosing scope).

TEST(ExpressionOpBuildScope, EchoOp) {
    auto ast = parseSrc("x = echo(\"val\") 1;");
    auto scope = buildScopes(ast);
    auto* echo = dynamic_cast<EchoOp*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(echo, nullptr);
    EXPECT_EQ(echo->scope(), scope.get());
    EXPECT_EQ(echo->body->scope(), scope.get());
}
TEST(ExpressionOpBuildScope, AssertOp) {
    auto ast = parseSrc("x = assert(true) 1;");
    auto scope = buildScopes(ast);
    auto* a = dynamic_cast<AssertOp*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->scope(), scope.get());
    EXPECT_EQ(a->body->scope(), scope.get());
}
TEST(ExpressionOpBuildScope, DivisionOp) {
    auto ast = parseSrc("x = 10 / 2;");
    auto scope = buildScopes(ast);
    auto* d = dynamic_cast<DivisionOp*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->scope(), scope.get());
    EXPECT_EQ(d->left->scope(), scope.get());
    EXPECT_EQ(d->right->scope(), scope.get());
}
TEST(ExpressionOpBuildScope, ModuloOp) {
    auto ast = parseSrc("x = 10 % 3;");
    auto scope = buildScopes(ast);
    auto* m = dynamic_cast<ModuloOp*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->left->scope(), scope.get());
    EXPECT_EQ(m->right->scope(), scope.get());
}
TEST(ExpressionOpBuildScope, ExponentOp) {
    auto ast = parseSrc("x = 2 ^ 3;");
    auto scope = buildScopes(ast);
    auto* e = dynamic_cast<ExponentOp*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->left->scope(), scope.get());
    EXPECT_EQ(e->right->scope(), scope.get());
}
TEST(ExpressionOpBuildScope, BitwiseAndOp) {
    auto ast = parseSrc("x = 5 & 3;");
    auto scope = buildScopes(ast);
    auto* op = dynamic_cast<BitwiseAndOp*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(op, nullptr);
    EXPECT_EQ(op->left->scope(), scope.get());
    EXPECT_EQ(op->right->scope(), scope.get());
}
TEST(ExpressionOpBuildScope, BitwiseOrOp) {
    auto ast = parseSrc("x = 5 | 3;");
    auto scope = buildScopes(ast);
    auto* op = dynamic_cast<BitwiseOrOp*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(op, nullptr);
    EXPECT_EQ(op->left->scope(), scope.get());
    EXPECT_EQ(op->right->scope(), scope.get());
}
TEST(ExpressionOpBuildScope, BitwiseNotOp) {
    auto ast = parseSrc("x = ~5;");
    auto scope = buildScopes(ast);
    auto* op = dynamic_cast<BitwiseNotOp*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(op, nullptr);
    EXPECT_EQ(op->expr->scope(), scope.get());
}
TEST(ExpressionOpBuildScope, BitwiseShiftLeftOp) {
    auto ast = parseSrc("x = 1 << 4;");
    auto scope = buildScopes(ast);
    auto* op = dynamic_cast<BitwiseShiftLeftOp*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(op, nullptr);
    EXPECT_EQ(op->left->scope(), scope.get());
    EXPECT_EQ(op->right->scope(), scope.get());
}
TEST(ExpressionOpBuildScope, BitwiseShiftRightOp) {
    auto ast = parseSrc("x = 16 >> 2;");
    auto scope = buildScopes(ast);
    auto* op = dynamic_cast<BitwiseShiftRightOp*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(op, nullptr);
    EXPECT_EQ(op->left->scope(), scope.get());
    EXPECT_EQ(op->right->scope(), scope.get());
}
TEST(ExpressionOpBuildScope, LogicalAndOp) {
    auto ast = parseSrc("x = true && false;");
    auto scope = buildScopes(ast);
    auto* op = dynamic_cast<LogicalAndOp*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(op, nullptr);
    EXPECT_EQ(op->left->scope(), scope.get());
    EXPECT_EQ(op->right->scope(), scope.get());
}
TEST(ExpressionOpBuildScope, LogicalOrOp) {
    auto ast = parseSrc("x = true || false;");
    auto scope = buildScopes(ast);
    auto* op = dynamic_cast<LogicalOrOp*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(op, nullptr);
    EXPECT_EQ(op->left->scope(), scope.get());
    EXPECT_EQ(op->right->scope(), scope.get());
}
TEST(ExpressionOpBuildScope, LogicalNotOp) {
    auto ast = parseSrc("x = !true;");
    auto scope = buildScopes(ast);
    auto* op = dynamic_cast<LogicalNotOp*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(op, nullptr);
    EXPECT_EQ(op->expr->scope(), scope.get());
}
TEST(ExpressionOpBuildScope, InequalityOp) {
    auto ast = parseSrc("x = 1 != 2;");
    auto scope = buildScopes(ast);
    auto* op = dynamic_cast<InequalityOp*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(op, nullptr);
    EXPECT_EQ(op->left->scope(), scope.get());
    EXPECT_EQ(op->right->scope(), scope.get());
}
TEST(ExpressionOpBuildScope, GreaterThanOrEqualOp) {
    auto ast = parseSrc("x = 1 >= 2;");
    auto scope = buildScopes(ast);
    auto* op = dynamic_cast<GreaterThanOrEqualOp*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(op, nullptr);
    EXPECT_EQ(op->left->scope(), scope.get());
    EXPECT_EQ(op->right->scope(), scope.get());
}
TEST(ExpressionOpBuildScope, LessThanOrEqualOp) {
    auto ast = parseSrc("x = 1 <= 2;");
    auto scope = buildScopes(ast);
    auto* op = dynamic_cast<LessThanOrEqualOp*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(op, nullptr);
    EXPECT_EQ(op->left->scope(), scope.get());
    EXPECT_EQ(op->right->scope(), scope.get());
}
TEST(ExpressionOpBuildScope, PrimaryIndex) {
    auto ast = parseSrc("x = v[0];");
    auto scope = buildScopes(ast);
    auto* idx = dynamic_cast<PrimaryIndex*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(idx, nullptr);
    EXPECT_EQ(idx->scope(), scope.get());
    EXPECT_EQ(idx->left->scope(), scope.get());
    EXPECT_EQ(idx->index->scope(), scope.get());
}
TEST(ExpressionOpBuildScope, PrimaryMember) {
    auto ast = parseSrc("x = v.x;");
    auto scope = buildScopes(ast);
    auto* mem = dynamic_cast<PrimaryMember*>(dynamic_cast<Assignment*>(ast[0].get())->expr.get());
    ASSERT_NE(mem, nullptr);
    EXPECT_EQ(mem->scope(), scope.get());
    EXPECT_EQ(mem->left->scope(), scope.get());
    EXPECT_EQ(mem->member->scope(), scope.get());
}

// -- intersection_for build_scope ----------------------------------------

TEST(IntersectionForBuildScope, Scope) {
    auto ast = parseSrc("intersection_for (i=[0:3]) cube(i);");
    auto scope = buildScopes(ast);
    auto* ifor = dynamic_cast<ModularIntersectionFor*>(ast[0].get());
    ASSERT_NE(ifor, nullptr);
    ASSERT_FALSE(ifor->body.empty());
    EXPECT_NE(ifor->body[0]->scope()->lookupVariable("i"), nullptr);
    EXPECT_EQ(scope->lookupVariable("i"), nullptr);
}

TEST(IntersectionForBuildScope, BlockBody) {
    auto ast = parseSrc("intersection_for (i=[0:3]) { cube(i); sphere(i); }");
    auto scope = buildScopes(ast);
    auto* ifor = dynamic_cast<ModularIntersectionFor*>(ast[0].get());
    ASSERT_NE(ifor, nullptr);
    for (auto& child : ifor->body) {
        EXPECT_NE(child->scope()->lookupVariable("i"), nullptr);
    }
}

// -- Hoisted module declaration -------------------------------------

TEST(HoistedModuleDeclarationTest, NestedModuleIsHoisted) {
    auto ast = parseSrc("module outer() {\n  inner();\n  module inner() { cube(1); }\n}");
    auto scope = buildScopes(ast);
    auto* outer = dynamic_cast<ModuleDeclaration*>(ast[0].get());
    ASSERT_NE(outer, nullptr);
    auto* callNode = dynamic_cast<ModularCall*>(outer->children[0].get());
    ASSERT_NE(callNode, nullptr);
    EXPECT_NE(callNode->scope()->lookupModule("inner"), nullptr);
}

TEST(HoistedModuleDeclarationTest, NestedModuleNotVisibleInOuterScope) {
    auto ast = parseSrc("module outer() {\n  module inner() { cube(1); }\n}");
    auto scope = buildScopes(ast);
    EXPECT_EQ(scope->lookupModule("inner"), nullptr);
    EXPECT_NE(scope->lookupModule("outer"), nullptr);
}
