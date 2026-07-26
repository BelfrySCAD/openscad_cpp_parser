%{
#include "driver.hpp"
#include "lexer_api.hpp"
#include "parser.tab.hpp"

#include <cstdlib>
#include <functional>
#include <string>
#include <unordered_map>

using oscad::OscadLocation;
using oscad::ParserDriver;

namespace {

void advanceCur(ParserDriver& driver, const char* text, int len) {
    for (int i = 0; i < len; ++i) {
        if (text[i] == '\n') {
            driver.cur.line++;
            driver.cur.column = 1;
        } else {
            driver.cur.column++;
        }
    }
    driver.cur.offset += len;
}

void updateLocation(ParserDriver& driver, const char* text, int len) {
    driver.tokenStart = driver.cur;
    advanceCur(driver, text, len);
    driver.tokenEnd = driver.cur;
}

using KeywordFactory = std::function<yy::parser::symbol_type(const OscadLocation&)>;

const std::unordered_map<std::string, KeywordFactory>& keywordTable() {
    static const std::unordered_map<std::string, KeywordFactory> table = {
        {"use", [](const OscadLocation& l) { return yy::parser::make_KW_USE(l); }},
        {"include", [](const OscadLocation& l) { return yy::parser::make_KW_INCLUDE(l); }},
        {"module", [](const OscadLocation& l) { return yy::parser::make_KW_MODULE(l); }},
        {"function", [](const OscadLocation& l) { return yy::parser::make_KW_FUNCTION(l); }},
        {"let", [](const OscadLocation& l) { return yy::parser::make_KW_LET(l); }},
        {"assert", [](const OscadLocation& l) { return yy::parser::make_KW_ASSERT(l); }},
        {"echo", [](const OscadLocation& l) { return yy::parser::make_KW_ECHO(l); }},
        {"if", [](const OscadLocation& l) { return yy::parser::make_KW_IF(l); }},
        {"else", [](const OscadLocation& l) { return yy::parser::make_KW_ELSE(l); }},
        {"for", [](const OscadLocation& l) { return yy::parser::make_KW_FOR(l); }},
        {"intersection_for", [](const OscadLocation& l) { return yy::parser::make_KW_INTERSECTION_FOR(l); }},
        {"each", [](const OscadLocation& l) { return yy::parser::make_KW_EACH(l); }},
        {"undef", [](const OscadLocation& l) { return yy::parser::make_KW_UNDEF(l); }},
        {"true", [](const OscadLocation& l) { return yy::parser::make_KW_TRUE(l); }},
        {"false", [](const OscadLocation& l) { return yy::parser::make_KW_FALSE(l); }},
    };
    return table;
}

} // namespace

#define YY_USER_ACTION updateLocation(driver, yytext, yyleng);
%}

/* nounistd: MSVC/Windows has no <unistd.h>; flex only includes it for
 * isatty()/POSIX I/O we never use (never-interactive already disables the
 * interactive-terminal codepath), so this is a no-op everywhere else. */
%option noyywrap nounput noinput never-interactive nounistd
%x STR
%x INCFILE

IDENT   \$?[_A-Za-z][A-Za-z0-9_]*
NUMLIT  0[xX][0-9A-Fa-f]+|[0-9]+([.][0-9]*)?([eE][+-]?[0-9]+)?|[.][0-9]+([eE][+-]?[0-9]+)?

%%

[ \t\r\n]+              { /* skip whitespace */ }
"//"[^\n]*               { /* skip line comment */ }
"/*"([^*]|\*+[^*/])*\*+"/" { /* skip block comment */ }

{IDENT} {
    auto loc = driver.currentTokenLoc();
    std::string text(yytext, yyleng);
    const auto& table = keywordTable();
    auto it = table.find(text);
    if (it != table.end()) {
        if (text == "use" || text == "include") {
            BEGIN(INCFILE);
        }
        return it->second(loc);
    }
    return yy::parser::make_NAME(std::move(text), loc);
}

{NUMLIT} { return yy::parser::make_NUMBER(std::strtod(yytext, nullptr), driver.currentTokenLoc()); }

\" {
    driver.stringBuffer.clear();
    BEGIN(STR);
}
<STR>\\. { driver.stringBuffer.append(yytext, yyleng); }
<STR>[^"\\]+ { driver.stringBuffer.append(yytext, yyleng); }
<STR>\" {
    BEGIN(INITIAL);
    return yy::parser::make_STRING(driver.stringBuffer, driver.currentTokenLoc());
}

<INCFILE>[ \t\r\n]+       { /* skip whitespace before < */ }
<INCFILE>"<"[^>]*">" {
    BEGIN(INITIAL);
    std::string path(yytext + 1, yyleng - 2);
    return yy::parser::make_USE_INCLUDE_FILE(std::move(path), driver.currentTokenLoc());
}

"<=" { return yy::parser::make_OP_LE(driver.currentTokenLoc()); }
">=" { return yy::parser::make_OP_GE(driver.currentTokenLoc()); }
"==" { return yy::parser::make_OP_EQ(driver.currentTokenLoc()); }
"!=" { return yy::parser::make_OP_NE(driver.currentTokenLoc()); }
"&&" { return yy::parser::make_OP_AND(driver.currentTokenLoc()); }
"||" { return yy::parser::make_OP_OR(driver.currentTokenLoc()); }
"<<" { return yy::parser::make_OP_SHL(driver.currentTokenLoc()); }
">>" { return yy::parser::make_OP_SHR(driver.currentTokenLoc()); }

"(" { return yy::parser::make_LPAREN(driver.currentTokenLoc()); }
")" { return yy::parser::make_RPAREN(driver.currentTokenLoc()); }
"{" { return yy::parser::make_LBRACE(driver.currentTokenLoc()); }
"}" { return yy::parser::make_RBRACE(driver.currentTokenLoc()); }
"[" { return yy::parser::make_LBRACKET(driver.currentTokenLoc()); }
"]" { return yy::parser::make_RBRACKET(driver.currentTokenLoc()); }
";" { return yy::parser::make_SEMI(driver.currentTokenLoc()); }
"," { return yy::parser::make_COMMA(driver.currentTokenLoc()); }
"=" { return yy::parser::make_ASSIGNOP(driver.currentTokenLoc()); }
":" { return yy::parser::make_COLON(driver.currentTokenLoc()); }
"?" { return yy::parser::make_QUESTION(driver.currentTokenLoc()); }
"." { return yy::parser::make_DOT(driver.currentTokenLoc()); }
"+" { return yy::parser::make_PLUS(driver.currentTokenLoc()); }
"-" { return yy::parser::make_MINUS(driver.currentTokenLoc()); }
"*" { return yy::parser::make_STAR(driver.currentTokenLoc()); }
"/" { return yy::parser::make_SLASH(driver.currentTokenLoc()); }
"%" { return yy::parser::make_PERCENT(driver.currentTokenLoc()); }
"^" { return yy::parser::make_CARET(driver.currentTokenLoc()); }
"&" { return yy::parser::make_AMP(driver.currentTokenLoc()); }
"|" { return yy::parser::make_PIPEOP(driver.currentTokenLoc()); }
"~" { return yy::parser::make_TILDE(driver.currentTokenLoc()); }
"!" { return yy::parser::make_BANG(driver.currentTokenLoc()); }
"<" { return yy::parser::make_LTOP(driver.currentTokenLoc()); }
">" { return yy::parser::make_GTOP(driver.currentTokenLoc()); }
"#" { return yy::parser::make_HASHOP(driver.currentTokenLoc()); }

. {
    driver.reportError(driver.currentTokenLoc(), std::string("unexpected character '") + yytext + "'");
}

<<EOF>> { return yy::parser::make_YYEOF(driver.currentTokenLoc()); }

%%

namespace oscad {

void lexerBeginString(const std::string& source) {
    yy_scan_bytes(source.data(), static_cast<int>(source.size()));
}

void lexerEnd() {
    yy_delete_buffer(YY_CURRENT_BUFFER);
}

} // namespace oscad
