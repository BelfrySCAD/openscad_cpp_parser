// Ported from openscad_lalr_parser/tests/test_nodes.py: constructs AST
// nodes directly (not via parsing) and checks toString() output.
//
// Two Python tests have no C++ equivalent and aren't ported:
// test_astnode_str_raises_not_implemented and
// test_vector_element_str_raises_not_implemented. Python's ASTNode/
// VectorElement are constructible dataclasses whose __str__ raises
// NotImplementedError at call time; our ASTNode/VectorElement are
// abstract classes (pure virtual toString()) that cannot be instantiated
// at all -- a compile error instead of a runtime one. Strictly stronger,
// nothing to test.
#include "openscad_cpp_parser/ast.hpp"

#include <gtest/gtest.h>

using namespace oscad;

namespace {

Position pos() {
    return Position{"<test>", 1, 1, 0, 0};
}
std::unique_ptr<Identifier> ident(const std::string& name) {
    return std::make_unique<Identifier>(pos(), name);
}
std::unique_ptr<Expression> num(double v) {
    return std::make_unique<NumberLiteral>(pos(), v);
}
std::unique_ptr<Assignment> assign(const std::string& name, std::unique_ptr<Expression> expr) {
    return std::make_unique<Assignment>(pos(), ident(name), std::move(expr));
}
std::vector<std::unique_ptr<Assignment>> assignList(std::unique_ptr<Assignment> a) {
    std::vector<std::unique_ptr<Assignment>> v;
    v.push_back(std::move(a));
    return v;
}
std::unique_ptr<ModularCall> cubeCall() {
    std::vector<std::unique_ptr<Argument>> args;
    args.push_back(std::make_unique<PositionalArgument>(pos(), num(1.0)));
    return std::make_unique<ModularCall>(pos(), ident("cube"), std::move(args), std::vector<std::unique_ptr<ASTNode>>{});
}
std::vector<std::unique_ptr<ASTNode>> singleton(std::unique_ptr<ASTNode> n) {
    std::vector<std::unique_ptr<ASTNode>> v;
    v.push_back(std::move(n));
    return v;
}

} // namespace

// -- Basic literals -----------------------------------------------------

TEST(NodeStr, BasicLiterals) {
    EXPECT_EQ(CommentLine(pos(), " hello").toString(), "// hello");
    EXPECT_EQ(CommentSpan(pos(), " block ").toString(), "/* block */");
    EXPECT_EQ(Identifier(pos(), "foo").toString(), "foo");
    EXPECT_EQ(StringLiteral(pos(), "bar").toString(), "\"bar\"");
    EXPECT_EQ(NumberLiteral(pos(), 1.5).toString(), "1.5");
    EXPECT_EQ(NumberLiteral(pos(), 1.0).toString(), "1");
    EXPECT_EQ(NumberLiteral(pos(), 0.0).toString(), "0");
    EXPECT_EQ(BooleanLiteral(pos(), true).toString(), "true");
    EXPECT_EQ(BooleanLiteral(pos(), false).toString(), "false");
    EXPECT_EQ(UndefinedLiteral(pos()).toString(), "undef");
}

// -- Params / args / assignments -----------------------------------------

TEST(NodeStr, ParamsArgsAssignments) {
    EXPECT_EQ(ParameterDeclaration(pos(), ident("x"), num(1.0)).toString(), "x=1");
    EXPECT_EQ(ParameterDeclaration(pos(), ident("y"), nullptr).toString(), "y");
    EXPECT_EQ(PositionalArgument(pos(), num(3.0)).toString(), "3");
    EXPECT_EQ(NamedArgument(pos(), ident("r"), num(5.0)).toString(), "r = 5");
    EXPECT_EQ(Assignment(pos(), ident("a"), num(10.0)).toString(), "a = 10");

    LetOp letOp(pos(), assignList(assign("x", num(1.0))), ident("x"));
    EXPECT_EQ(letOp.toString(), "let(x = 1) x");

    std::vector<std::unique_ptr<Argument>> echoArgs;
    echoArgs.push_back(std::make_unique<PositionalArgument>(pos(), num(1.0)));
    EchoOp echoOp(pos(), std::move(echoArgs), num(2.0));
    EXPECT_EQ(echoOp.toString(), "echo(1) 2");

    std::vector<std::unique_ptr<Argument>> assertArgs;
    assertArgs.push_back(std::make_unique<PositionalArgument>(pos(), std::make_unique<BooleanLiteral>(pos(), true)));
    AssertOp assertOp(pos(), std::move(assertArgs), num(3.0));
    EXPECT_EQ(assertOp.toString(), "assert(true) 3");
}

// -- Operators ------------------------------------------------------------

TEST(NodeStr, Operators) {
    EXPECT_EQ(UnaryMinusOp(pos(), num(1.0)).toString(), "-1");
    EXPECT_EQ(AdditionOp(pos(), num(1.0), num(2.0)).toString(), "1 + 2");
    EXPECT_EQ(SubtractionOp(pos(), num(1.0), num(2.0)).toString(), "1 - 2");
    EXPECT_EQ(MultiplicationOp(pos(), num(1.0), num(2.0)).toString(), "1 * 2");
    EXPECT_EQ(DivisionOp(pos(), num(1.0), num(2.0)).toString(), "1 / 2");
    EXPECT_EQ(ModuloOp(pos(), num(1.0), num(2.0)).toString(), "1 % 2");
    EXPECT_EQ(ExponentOp(pos(), num(1.0), num(2.0)).toString(), "1 ^ 2");
    EXPECT_EQ(BitwiseAndOp(pos(), num(1.0), num(2.0)).toString(), "1 & 2");
    EXPECT_EQ(BitwiseOrOp(pos(), num(1.0), num(2.0)).toString(), "1 | 2");
    EXPECT_EQ(BitwiseNotOp(pos(), num(1.0)).toString(), "~1");
    EXPECT_EQ(BitwiseShiftLeftOp(pos(), num(1.0), num(2.0)).toString(), "1 << 2");
    EXPECT_EQ(BitwiseShiftRightOp(pos(), num(1.0), num(2.0)).toString(), "1 >> 2");
    EXPECT_EQ(LogicalAndOp(pos(), num(1.0), num(2.0)).toString(), "1 && 2");
    EXPECT_EQ(LogicalOrOp(pos(), num(1.0), num(2.0)).toString(), "1 || 2");
    EXPECT_EQ(LogicalNotOp(pos(), num(1.0)).toString(), "!1");
    EXPECT_EQ(TernaryOp(pos(), num(1.0), num(2.0), num(3.0)).toString(), "1 ? 2 : 3");
    EXPECT_EQ(EqualityOp(pos(), num(1.0), num(2.0)).toString(), "1 == 2");
    EXPECT_EQ(InequalityOp(pos(), num(1.0), num(2.0)).toString(), "1 != 2");
    EXPECT_EQ(GreaterThanOp(pos(), num(1.0), num(2.0)).toString(), "1 > 2");
    EXPECT_EQ(GreaterThanOrEqualOp(pos(), num(1.0), num(2.0)).toString(), "1 >= 2");
    EXPECT_EQ(LessThanOp(pos(), num(1.0), num(2.0)).toString(), "1 < 2");
    EXPECT_EQ(LessThanOrEqualOp(pos(), num(1.0), num(2.0)).toString(), "1 <= 2");
}

// -- Primary and range ------------------------------------------------

TEST(NodeStr, PrimaryAndRange) {
    std::vector<std::unique_ptr<ParameterDeclaration>> params;
    params.push_back(std::make_unique<ParameterDeclaration>(pos(), ident("x"), nullptr));
    FunctionLiteral fl(pos(), std::move(params), num(4.0));
    EXPECT_EQ(fl.toString(), "function(x) 4");

    std::vector<std::unique_ptr<Argument>> args;
    args.push_back(std::make_unique<PositionalArgument>(pos(), num(3.0)));
    PrimaryCall call(pos(), ident("foo"), std::move(args));
    EXPECT_EQ(call.toString(), "foo(3)");

    PrimaryIndex idx(pos(), ident("arr"), num(1.0));
    EXPECT_EQ(idx.toString(), "arr[1]");

    PrimaryMember mem(pos(), ident("obj"), ident("x"));
    EXPECT_EQ(mem.toString(), "obj.x");

    RangeLiteral range(pos(), num(0.0), num(5.0), num(1.0));
    EXPECT_EQ(range.toString(), "[0 : 1 : 5]");
}

// -- List comprehension elements -------------------------------------

TEST(NodeStr, ListCompLetContainsLetSubstring) {
    // Mirrors the reference's own loose check (`"let" in str(...)`) rather
    // than a deep nested construction -- the Python test's exact nesting
    // was arbitrary and not itself asserted beyond this substring.
    ListCompLet lcLet(pos(), assignList(assign("x", num(1.0))), ident("x"));
    EXPECT_NE(lcLet.toString().find("let"), std::string::npos);
}

TEST(NodeStr, ListCompEach) {
    ListCompEach e(pos(), num(1.0));
    EXPECT_EQ(e.toString(), "each 1");
}

TEST(NodeStr, ListCompFor) {
    ListCompFor f(pos(), assignList(assign("i", ident("list"))), ident("i"));
    EXPECT_EQ(f.toString(), "for (i = list) i");
}

TEST(NodeStr, ListCompCFor) {
    std::vector<std::unique_ptr<Assignment>> inits;
    inits.push_back(assign("i", num(0.0)));
    auto condition = std::make_unique<LessThanOp>(pos(), ident("i"), num(10.0));
    std::vector<std::unique_ptr<Assignment>> incrs;
    incrs.push_back(assign("i", std::make_unique<AdditionOp>(pos(), ident("i"), num(1.0))));
    ListCompCFor cfor(pos(), std::move(inits), std::move(condition), std::move(incrs), ident("i"));
    EXPECT_EQ(cfor.toString(), "for (i = 0; i < 10; i = i + 1) i");
}

TEST(NodeStr, ListCompIf) {
    ListCompIf ifElem(pos(), num(1.0), num(5.0));
    EXPECT_EQ(ifElem.toString(), "if (1) 5");
}

TEST(NodeStr, ListCompIfElse) {
    ListCompIfElse ifElse(pos(), num(1.0), num(6.0), num(7.0));
    EXPECT_EQ(ifElse.toString(), "if (1) 6 else 7");
}

TEST(NodeStr, ListComprehensionWrappingIf) {
    std::vector<std::unique_ptr<ASTNode>> elements;
    elements.push_back(std::make_unique<ListCompIf>(pos(), num(1.0), num(5.0)));
    ListComprehension lc(pos(), std::move(elements));
    EXPECT_EQ(lc.toString(), "[if (1) 5]");
}

// -- Modular and declaration nodes -------------------------------------

TEST(NodeStr, ModularAndDeclarationNodes) {
    EXPECT_EQ(cubeCall()->toString(), "cube(1)");

    ModularFor forNode(pos(), assignList(assign("i", ident("list"))), singleton(cubeCall()));
    EXPECT_EQ(forNode.toString(), "for (i = list) cube(1)");

    ModularIntersectionFor iforNode(pos(), assignList(assign("i", ident("list"))), singleton(cubeCall()));
    EXPECT_EQ(iforNode.toString(), "intersection_for (i = list) cube(1)");

    ModularLet letNode(pos(), assignList(assign("x", num(1.0))), singleton(cubeCall()));
    EXPECT_EQ(letNode.toString(), "let (x = 1) cube(1)");

    std::vector<std::unique_ptr<Argument>> echoArgs;
    echoArgs.push_back(std::make_unique<PositionalArgument>(pos(), num(1.0)));
    ModularEcho echoNode(pos(), std::move(echoArgs), singleton(cubeCall()));
    EXPECT_EQ(echoNode.toString(), "echo(1) cube(1)");

    std::vector<std::unique_ptr<Argument>> assertArgs;
    assertArgs.push_back(std::make_unique<PositionalArgument>(pos(), std::make_unique<BooleanLiteral>(pos(), true)));
    ModularAssert assertNode(pos(), std::move(assertArgs), singleton(cubeCall()));
    EXPECT_EQ(assertNode.toString(), "assert(true) cube(1)");

    ModularIf ifNode(pos(), num(1.0), singleton(cubeCall()));
    EXPECT_EQ(ifNode.toString(), "if (1) cube(1)");

    ModularIfElse ifElseNode(pos(), num(1.0), singleton(cubeCall()), singleton(cubeCall()));
    EXPECT_EQ(ifElseNode.toString(), "if (1) cube(1) else cube(1)");

    EXPECT_EQ(ModularModifierShowOnly(pos(), cubeCall()).toString(), "!cube(1)");
    EXPECT_EQ(ModularModifierHighlight(pos(), cubeCall()).toString(), "#cube(1)");
    EXPECT_EQ(ModularModifierBackground(pos(), cubeCall()).toString(), "%cube(1)");
    EXPECT_EQ(ModularModifierDisable(pos(), cubeCall()).toString(), "*cube(1)");

    std::vector<std::unique_ptr<ParameterDeclaration>> noParams;
    std::vector<std::unique_ptr<ASTNode>> moduleChildren;
    moduleChildren.push_back(cubeCall());
    ModuleDeclaration modDecl(pos(), ident("m"), std::move(noParams), std::move(moduleChildren));
    EXPECT_EQ(modDecl.toString(), "module m() { cube(1) }");

    std::vector<std::unique_ptr<ParameterDeclaration>> noParams2;
    FunctionDeclaration funcDecl(pos(), ident("f"), std::move(noParams2), num(2.0));
    EXPECT_EQ(funcDecl.toString(), "function f() = 2;");

    UseStatement useStmt(pos(), std::make_unique<StringLiteral>(pos(), "lib.scad"));
    EXPECT_EQ(useStmt.toString(), "use <lib.scad>");

    IncludeStatement includeStmt(pos(), std::make_unique<StringLiteral>(pos(), "lib.scad"));
    EXPECT_EQ(includeStmt.toString(), "include <lib.scad>");
}
