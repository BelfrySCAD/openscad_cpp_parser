%require "3.8"
%language "c++"
%skeleton "lalr1.cc"

%define api.token.constructor
%define api.value.type variant
%define parse.error verbose
%locations

// Two dangling-else shift/reduce conflicts, both resolved by the default
// shift preference (binds to the innermost `if`, confirmed by
// test_smoke.cpp's DanglingElseAttachesToInnerIf): one for module-level
// if/else, one for the identical ambiguity in list-comprehension if/else.
// If this count changes, a grammar edit introduced (or removed) a real
// ambiguity -- investigate before adjusting the number.
%expect 2

%code requires {
  #include "openscad_cpp_parser/ast.hpp"
  #include "oscad_location.hpp"
  #include <memory>
  #include <string>
  #include <vector>

  namespace oscad {
    using NodePtr = std::unique_ptr<ASTNode>;
    using NodeList = std::vector<std::unique_ptr<ASTNode>>;
    class ParserDriver;
  }
  // The generated parser.tab.hpp emits %type names (NodePtr/NodeList)
  // unqualified, outside the oscad namespace -- bring them into global
  // scope too so those references resolve.
  using oscad::NodePtr;
  using oscad::NodeList;
}

%define api.location.type {oscad::OscadLocation}

%param { oscad::ParserDriver& driver }

%code top {
  // Custom flat (first_line/last_line/...) location type -- see
  // oscad_location.hpp -- requires overriding bison's default
  // begin/end-based merging macro.
  #define YYLLOC_DEFAULT(Cur, Rhs, N)                                        \
    do {                                                                      \
      if (N) {                                                                \
        (Cur).first_line   = YYRHSLOC(Rhs, 1).first_line;                     \
        (Cur).first_column = YYRHSLOC(Rhs, 1).first_column;                   \
        (Cur).first_offset = YYRHSLOC(Rhs, 1).first_offset;                   \
        (Cur).last_line    = YYRHSLOC(Rhs, N).last_line;                      \
        (Cur).last_column  = YYRHSLOC(Rhs, N).last_column;                    \
        (Cur).last_offset  = YYRHSLOC(Rhs, N).last_offset;                    \
      } else {                                                                \
        (Cur).first_line = (Cur).last_line = YYRHSLOC(Rhs, 0).last_line;      \
        (Cur).first_column = (Cur).last_column = YYRHSLOC(Rhs, 0).last_column; \
        (Cur).first_offset = (Cur).last_offset = YYRHSLOC(Rhs, 0).last_offset; \
      }                                                                       \
    } while (false)
}

%code {
  #include "driver.hpp"
  using namespace oscad;
}

%code provides {
  // Tells flex the exact signature to emit for the scanner function, and
  // declares it here (in the bison-generated header, included by both
  // lexer.l and parser.tab.cpp) so both sides agree on linkage.
  #define YY_DECL yy::parser::symbol_type yylex(oscad::ParserDriver& driver)
  YY_DECL;
}

%token
  KW_USE "use"
  KW_INCLUDE "include"
  KW_MODULE "module"
  KW_FUNCTION "function"
  KW_LET "let"
  KW_ASSERT "assert"
  KW_ECHO "echo"
  KW_IF "if"
  KW_ELSE "else"
  KW_FOR "for"
  KW_INTERSECTION_FOR "intersection_for"
  KW_EACH "each"
  KW_UNDEF "undef"
  KW_TRUE "true"
  KW_FALSE "false"
  LPAREN "("
  RPAREN ")"
  LBRACE "{"
  RBRACE "}"
  LBRACKET "["
  RBRACKET "]"
  SEMI ";"
  COMMA ","
  ASSIGNOP "="
  COLON ":"
  QUESTION "?"
  DOT "."
  PLUS "+"
  MINUS "-"
  STAR "*"
  SLASH "/"
  PERCENT "%"
  CARET "^"
  AMP "&"
  PIPEOP "|"
  TILDE "~"
  BANG "!"
  LTOP "<"
  GTOP ">"
  HASHOP "#"
  OP_LE "<="
  OP_GE ">="
  OP_EQ "=="
  OP_NE "!="
  OP_AND "&&"
  OP_OR "||"
  OP_SHL "<<"
  OP_SHR ">>"
;

%token <std::string> NAME STRING USE_INCLUDE_FILE
%token <double> NUMBER

%left "||"
%left "&&"
%left "==" "!="
%left "<" ">" "<=" ">="
%left "|"
%left "&"
%left "<<" ">>"
%left "+" "-"
%left "*" "/" "%"
%precedence UMINUS UNOT UBNOT UPLUS
%right "^"

%type <NodeList> program toplevel_statement_star toplevel_statement
%type <NodeList> statement statement_star statement_block child_statement
%type <NodeList> parameters parameter_seq arguments argument_seq
%type <NodeList> assignments_expr assignment_expr_seq vector_elements

%type <NodePtr> assignment_expr parameter argument
%type <NodePtr> module_definition function_definition assignment
%type <NodePtr> use_statement include_statement
%type <NodePtr> module_instantiation single_module_instantiation
%type <NodePtr> modifier_show_only modifier_highlight modifier_background modifier_disable
%type <NodePtr> if_statement ifelse_statement
%type <NodePtr> modular_for modular_intersection_for modular_let modular_assert modular_echo modular_call
%type <NodePtr> expr opchain postfix primary
%type <NodePtr> range_expr vector_expr vector_element
%type <NodePtr> listcomp_elements listcomp_paren_expr listcomp_let listcomp_each
%type <NodePtr> listcomp_for listcomp_c_for listcomp_ifonly listcomp_ifelse

%%

program:
    toplevel_statement_star { driver.result = std::move($1); }
  ;

toplevel_statement_star:
    %empty                                  { $$ = NodeList{}; }
  | toplevel_statement_star toplevel_statement {
      $$ = std::move($1);
      for (auto& n : $2) $$.push_back(std::move(n));
    }
  ;

toplevel_statement:
    use_statement     { $$ = NodeList{}; $$.push_back(std::move($1)); }
  | include_statement  { $$ = NodeList{}; $$.push_back(std::move($1)); }
  | statement           { $$ = std::move($1); }
  ;

use_statement: "use" USE_INCLUDE_FILE { $$ = makeUseStatement(driver, @$, std::move($2)); } ;
include_statement: "include" USE_INCLUDE_FILE { $$ = makeIncludeStatement(driver, @$, std::move($2)); } ;

statement:
    ";"                    { $$ = NodeList{}; }
  | statement_block         { $$ = std::move($1); }
  | module_definition        { $$ = NodeList{}; $$.push_back(std::move($1)); }
  | function_definition       { $$ = NodeList{}; $$.push_back(std::move($1)); }
  | module_instantiation       { $$ = NodeList{}; $$.push_back(std::move($1)); }
  | assignment                  { $$ = NodeList{}; $$.push_back(std::move($1)); }
  ;

statement_block: "{" statement_star "}" { $$ = std::move($2); } ;

statement_star:
    %empty                     { $$ = NodeList{}; }
  | statement_star statement    {
      $$ = std::move($1);
      for (auto& n : $2) $$.push_back(std::move(n));
    }
  ;

child_statement:
    ";"                    { $$ = NodeList{}; }
  | statement_block         { $$ = std::move($1); }
  | module_instantiation      { $$ = NodeList{}; $$.push_back(std::move($1)); }
  ;

module_definition:
    "module" NAME "(" parameters ")" statement {
      $$ = makeModuleDeclaration(driver, @$, @2, std::move($2), std::move($4), std::move($6));
    }
  ;

function_definition:
    "function" NAME "(" parameters ")" "=" expr ";" {
      $$ = makeFunctionDeclaration(driver, @$, @2, std::move($2), std::move($4), std::move($7));
    }
  ;

assignment:
    NAME "=" expr ";" { $$ = makeAssignment(driver, @$, @1, std::move($1), std::move($3)); }
  ;

parameters:
    %empty                     { $$ = NodeList{}; }
  | parameter_seq               { $$ = std::move($1); }
  | parameter_seq ","            { $$ = std::move($1); }
  ;

parameter_seq:
    parameter                        { $$ = NodeList{}; $$.push_back(std::move($1)); }
  | parameter_seq "," parameter        { $$ = std::move($1); $$.push_back(std::move($3)); }
  ;

parameter:
    NAME "=" expr { $$ = makeParameterDeclaration(driver, @$, @1, std::move($1), std::move($3)); }
  | NAME           { $$ = makeParameterDeclaration(driver, @$, @1, std::move($1), nullptr); }
  ;

arguments:
    %empty                    { $$ = NodeList{}; }
  | argument_seq               { $$ = std::move($1); }
  | argument_seq ","            { $$ = std::move($1); }
  ;

argument_seq:
    argument                       { $$ = NodeList{}; $$.push_back(std::move($1)); }
  | argument_seq "," argument        { $$ = std::move($1); $$.push_back(std::move($3)); }
  ;

argument:
    NAME "=" expr { $$ = makeNamedArgument(driver, @$, @1, std::move($1), std::move($3)); }
  | expr           { $$ = makePositionalArgument(driver, @$, std::move($1)); }
  ;

assignments_expr:
    %empty                          { $$ = NodeList{}; }
  | assignment_expr_seq              { $$ = std::move($1); }
  | assignment_expr_seq ","           { $$ = std::move($1); }
  ;

assignment_expr_seq:
    assignment_expr                            { $$ = NodeList{}; $$.push_back(std::move($1)); }
  | assignment_expr_seq "," assignment_expr      { $$ = std::move($1); $$.push_back(std::move($3)); }
  ;

assignment_expr:
    NAME "=" expr { $$ = makeAssignment(driver, @$, @1, std::move($1), std::move($3)); }
  ;

module_instantiation:
    modifier_show_only        { $$ = std::move($1); }
  | modifier_highlight         { $$ = std::move($1); }
  | modifier_background        { $$ = std::move($1); }
  | modifier_disable           { $$ = std::move($1); }
  | ifelse_statement            { $$ = std::move($1); }
  | if_statement                 { $$ = std::move($1); }
  | single_module_instantiation   { $$ = std::move($1); }
  ;

modifier_show_only:   "!" module_instantiation { $$ = makeModifier<ModularModifierShowOnly>(driver, @$, std::move($2)); } ;
modifier_highlight:   "#" module_instantiation { $$ = makeModifier<ModularModifierHighlight>(driver, @$, std::move($2)); } ;
modifier_background:  "%" module_instantiation { $$ = makeModifier<ModularModifierBackground>(driver, @$, std::move($2)); } ;
modifier_disable:     "*" module_instantiation { $$ = makeModifier<ModularModifierDisable>(driver, @$, std::move($2)); } ;

if_statement:
    "if" "(" expr ")" child_statement { $$ = makeModularIf(driver, @$, std::move($3), std::move($5)); }
  ;

ifelse_statement:
    "if" "(" expr ")" child_statement "else" child_statement {
      $$ = makeModularIfElse(driver, @$, std::move($3), std::move($5), std::move($7));
    }
  ;

single_module_instantiation:
    modular_for              { $$ = std::move($1); }
  | modular_intersection_for   { $$ = std::move($1); }
  | modular_let                 { $$ = std::move($1); }
  | modular_assert               { $$ = std::move($1); }
  | modular_echo                  { $$ = std::move($1); }
  | modular_call                   { $$ = std::move($1); }
  ;

modular_for:
    "for" "(" assignments_expr ")" child_statement {
      $$ = makeModularFor(driver, @$, std::move($3), std::move($5));
    }
  ;

modular_intersection_for:
    "intersection_for" "(" assignments_expr ")" child_statement {
      $$ = makeModularIntersectionFor(driver, @$, std::move($3), std::move($5));
    }
  ;

modular_let:
    "let" "(" assignments_expr ")" child_statement {
      $$ = makeModularLet(driver, @$, std::move($3), std::move($5));
    }
  ;

modular_assert:
    "assert" "(" arguments ")" child_statement {
      $$ = makeModularAssert(driver, @$, std::move($3), std::move($5));
    }
  ;

modular_echo:
    "echo" "(" arguments ")" child_statement {
      $$ = makeModularEcho(driver, @$, std::move($3), std::move($5));
    }
  ;

modular_call:
    NAME "(" arguments ")" child_statement {
      $$ = makeModularCall(driver, @$, @1, std::move($1), std::move($3), std::move($5));
    }
  ;

// -- Expressions ----------------------------------------------------------
//
// `expr` covers let/assert/echo/funclit_def/ternary plus the operator
// cascade (`opchain`). let_expr/assert_expr/echo_expr/funclit_def are only
// reachable as the WHOLE of `expr`, not from inside `opchain` -- so
// `1 + let(x=2) x` stays a syntax error, matching the reference grammar.
// (A parenthesized `(let(x=2) x)` still works via primary's `"(" expr ")"`,
// which re-enters the full `expr` rule.)

expr:
    "let" "(" assignments_expr ")" expr { $$ = makeLetOp(driver, @$, std::move($3), std::move($5)); }
  | "assert" "(" arguments ")" expr      { $$ = makeAssertOp(driver, @$, std::move($3), std::move($5)); }
  | "assert" "(" arguments ")"            { $$ = makeAssertOp(driver, @$, std::move($3), makeUndefinedLiteral(driver, @$)); }
  | "echo" "(" arguments ")" expr          { $$ = makeEchoOp(driver, @$, std::move($3), std::move($5)); }
  | "echo" "(" arguments ")"                { $$ = makeEchoOp(driver, @$, std::move($3), makeUndefinedLiteral(driver, @$)); }
  | "function" "(" parameters ")" expr        { $$ = makeFunctionLiteral(driver, @$, std::move($3), std::move($5)); }
  | opchain "?" expr ":" expr                  { $$ = makeTernaryOp(driver, @$, std::move($1), std::move($3), std::move($5)); }
  | opchain                                     { $$ = std::move($1); }
  ;

opchain:
    opchain "||" opchain { $$ = makeBinaryOp<LogicalOrOp>(driver, @$, std::move($1), std::move($3)); }
  | opchain "&&" opchain { $$ = makeBinaryOp<LogicalAndOp>(driver, @$, std::move($1), std::move($3)); }
  | opchain "==" opchain { $$ = makeBinaryOp<EqualityOp>(driver, @$, std::move($1), std::move($3)); }
  | opchain "!=" opchain { $$ = makeBinaryOp<InequalityOp>(driver, @$, std::move($1), std::move($3)); }
  | opchain "<" opchain  { $$ = makeBinaryOp<LessThanOp>(driver, @$, std::move($1), std::move($3)); }
  | opchain ">" opchain  { $$ = makeBinaryOp<GreaterThanOp>(driver, @$, std::move($1), std::move($3)); }
  | opchain "<=" opchain { $$ = makeBinaryOp<LessThanOrEqualOp>(driver, @$, std::move($1), std::move($3)); }
  | opchain ">=" opchain { $$ = makeBinaryOp<GreaterThanOrEqualOp>(driver, @$, std::move($1), std::move($3)); }
  | opchain "|" opchain  { $$ = makeBinaryOp<BitwiseOrOp>(driver, @$, std::move($1), std::move($3)); }
  | opchain "&" opchain  { $$ = makeBinaryOp<BitwiseAndOp>(driver, @$, std::move($1), std::move($3)); }
  | opchain "<<" opchain { $$ = makeBinaryOp<BitwiseShiftLeftOp>(driver, @$, std::move($1), std::move($3)); }
  | opchain ">>" opchain { $$ = makeBinaryOp<BitwiseShiftRightOp>(driver, @$, std::move($1), std::move($3)); }
  | opchain "+" opchain  { $$ = makeBinaryOp<AdditionOp>(driver, @$, std::move($1), std::move($3)); }
  | opchain "-" opchain  { $$ = makeBinaryOp<SubtractionOp>(driver, @$, std::move($1), std::move($3)); }
  | opchain "*" opchain  { $$ = makeBinaryOp<MultiplicationOp>(driver, @$, std::move($1), std::move($3)); }
  | opchain "/" opchain  { $$ = makeBinaryOp<DivisionOp>(driver, @$, std::move($1), std::move($3)); }
  | opchain "%" opchain  { $$ = makeBinaryOp<ModuloOp>(driver, @$, std::move($1), std::move($3)); }
  | opchain "^" opchain  { $$ = makeBinaryOp<ExponentOp>(driver, @$, std::move($1), std::move($3)); }
  | "-" opchain %prec UMINUS { $$ = makeUnaryOp<UnaryMinusOp>(driver, @$, std::move($2)); }
  | "!" opchain %prec UNOT   { $$ = makeUnaryOp<LogicalNotOp>(driver, @$, std::move($2)); }
  | "~" opchain %prec UBNOT  { $$ = makeUnaryOp<BitwiseNotOp>(driver, @$, std::move($2)); }
  | "+" opchain %prec UPLUS  { $$ = std::move($2); /* unary plus is a no-op: no AST node */ }
  | postfix                  { $$ = std::move($1); }
  ;

postfix:
    primary                          { $$ = std::move($1); }
  | postfix "(" arguments ")"         { $$ = makePrimaryCall(driver, @$, std::move($1), std::move($3)); }
  | postfix "[" expr "]"               { $$ = makePrimaryIndex(driver, @$, std::move($1), std::move($3)); }
  | postfix "." NAME                    { $$ = makePrimaryMember(driver, @$, std::move($1), @3, std::move($3)); }
  ;

primary:
    "(" expr ")"       { $$ = std::move($2); }
  | range_expr           { $$ = std::move($1); }
  | vector_expr            { $$ = std::move($1); }
  | "undef"                 { $$ = makeUndefinedLiteral(driver, @$); }
  | "true"                    { $$ = makeBooleanLiteral(driver, @$, true); }
  | "false"                    { $$ = makeBooleanLiteral(driver, @$, false); }
  | STRING                       { $$ = makeStringLiteral(driver, @$, std::move($1)); }
  | NUMBER                         { $$ = makeNumberLiteral(driver, @$, $1); }
  | NAME                             { $$ = makeIdentifier(driver, @$, std::move($1)); }
  ;

range_expr:
    "[" expr ":" expr "]"           { $$ = makeRangeLiteral(driver, @$, std::move($2), std::move($4), nullptr); }
  | "[" expr ":" expr ":" expr "]"   { $$ = makeRangeLiteral(driver, @$, std::move($2), std::move($6), std::move($4)); }
  ;

vector_expr:
    "[" "]"                  { $$ = makeListComprehension(driver, @$, NodeList{}); }
  | "[" vector_elements "]"   { $$ = makeListComprehension(driver, @$, std::move($2)); }
  ;

vector_elements:
    vector_element                          { $$ = NodeList{}; $$.push_back(std::move($1)); }
  | vector_elements "," vector_element        { $$ = std::move($1); $$.push_back(std::move($3)); }
  | vector_elements ","                        { $$ = std::move($1); }
  ;

vector_element:
    listcomp_elements  { $$ = std::move($1); }
  | expr                { $$ = std::move($1); }
  ;

listcomp_elements:
    listcomp_paren_expr  { $$ = std::move($1); }
  | listcomp_let           { $$ = std::move($1); }
  | listcomp_each           { $$ = std::move($1); }
  | listcomp_c_for           { $$ = std::move($1); }
  | listcomp_for               { $$ = std::move($1); }
  | listcomp_ifelse             { $$ = std::move($1); }
  | listcomp_ifonly              { $$ = std::move($1); }
  ;

listcomp_paren_expr: "(" listcomp_elements ")" { $$ = std::move($2); } ;

listcomp_let:
    "let" "(" assignments_expr ")" listcomp_elements {
      $$ = makeListCompLet(driver, @$, std::move($3), std::move($5));
    }
  ;

listcomp_each: "each" vector_element { $$ = makeListCompEach(driver, @$, std::move($2)); } ;

listcomp_for:
    "for" "(" assignments_expr ")" vector_element {
      $$ = makeListCompFor(driver, @$, std::move($3), std::move($5));
    }
  ;

listcomp_c_for:
    "for" "(" assignments_expr ";" expr ";" assignments_expr ")" vector_element {
      $$ = makeListCompCFor(driver, @$, std::move($3), std::move($5), std::move($7), std::move($9));
    }
  ;

listcomp_ifonly:
    "if" "(" expr ")" vector_element { $$ = makeListCompIf(driver, @$, std::move($3), std::move($5)); }
  ;

listcomp_ifelse:
    "if" "(" expr ")" vector_element "else" vector_element {
      $$ = makeListCompIfElse(driver, @$, std::move($3), std::move($5), std::move($7));
    }
  ;

%%

void yy::parser::error(const location_type& loc, const std::string& message) {
    driver.reportError(loc, message);
}
