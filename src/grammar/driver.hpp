#pragma once

#include "oscad_location.hpp"
#include "openscad_cpp_parser/ast.hpp"

#include <memory>
#include <string>
#include <vector>

namespace oscad {

using NodePtr = std::unique_ptr<ASTNode>;
using NodeList = std::vector<std::unique_ptr<ASTNode>>;

// Downcasts a NodePtr known (by grammar construction) to actually hold a
// `Derived`. Not checked at runtime -- the grammar guarantees the dynamic
// type, this just recovers the static type C++ needs to call Derived's
// constructor.
template <typename Derived, typename Base>
std::unique_ptr<Derived> nodeCast(std::unique_ptr<Base> node) {
    return std::unique_ptr<Derived>(static_cast<Derived*>(node.release()));
}

// Lexer's running position, in (1-indexed line, 1-indexed column, 0-based
// byte offset).
struct LexPos {
    int line = 1;
    int column = 1;
    int offset = 0;
};

// Owns parse-time state: the origin string (for Position::origin), the
// lexer's running position, the resulting top-level node list, and error
// reporting. One ParserDriver per parseAst() call.
class ParserDriver {
public:
    explicit ParserDriver(std::string origin) : origin_(std::move(origin)) {}

    std::string origin_;
    NodeList result;
    bool hadError = false;
    int errorLine = 0;
    int errorColumn = 0;
    int errorOffset = 0;
    std::string errorReason; // bison's raw "syntax error, unexpected X, expecting Y" text

    LexPos cur;
    LexPos tokenStart;
    LexPos tokenEnd;
    std::string stringBuffer; // accumulates a STRING token's content while in the %x STR lexer state

    // Where the opening quote of the STRING currently being lexed began.
    //
    // Needed because YY_USER_ACTION resets tokenStart on EVERY rule match,
    // and a string is matched by several rules (opening quote, content
    // runs, escapes, closing quote). currentTokenLoc() at the closing
    // quote therefore describes just that one character, which made every
    // StringLiteral's source span a single `"` -- and dragged the span of
    // anything wrapping it (a PositionalArgument, a vector element) along
    // with it. Recorded when the opening quote is matched and used by
    // stringTokenLoc() below.
    LexPos stringStart;

    Position toPosition(const OscadLocation& loc) const {
        return Position{origin_, loc.first_line, loc.first_column, loc.first_offset, loc.last_offset};
    }

    OscadLocation currentTokenLoc() const {
        return OscadLocation{tokenStart.line,  tokenStart.column, tokenEnd.line,
                              tokenEnd.column, tokenStart.offset, tokenEnd.offset};
    }

    // As currentTokenLoc(), but spanning from the string's OPENING quote
    // (stringStart) to the current token's end -- i.e. the whole literal
    // including both quotes, which is what a caller slicing the source by
    // this span expects to get back.
    OscadLocation stringTokenLoc() const {
        return OscadLocation{stringStart.line,  stringStart.column, tokenEnd.line,
                              tokenEnd.column,  stringStart.offset, tokenEnd.offset};
    }

    void reportError(const OscadLocation& loc, const std::string& message);
};

// -- Node-construction helpers used by parser.y's actions -----------------

std::unique_ptr<Identifier> makeIdentifier(ParserDriver& driver, const OscadLocation& loc, std::string name);
NodePtr makeStringLiteral(ParserDriver& driver, const OscadLocation& loc, std::string val);
NodePtr makeNumberLiteral(ParserDriver& driver, const OscadLocation& loc, double val);
NodePtr makeBooleanLiteral(ParserDriver& driver, const OscadLocation& loc, bool val);
NodePtr makeUndefinedLiteral(ParserDriver& driver, const OscadLocation& loc);
NodePtr makeRangeLiteral(ParserDriver& driver, const OscadLocation& loc, NodePtr start, NodePtr end, NodePtr step);

NodePtr makePositionalArgument(ParserDriver& driver, const OscadLocation& loc, NodePtr expr);
NodePtr makeNamedArgument(ParserDriver& driver, const OscadLocation& loc, const OscadLocation& nameLoc,
                           std::string name, NodePtr expr);
NodePtr makeParameterDeclaration(ParserDriver& driver, const OscadLocation& loc, const OscadLocation& nameLoc,
                                  std::string name, NodePtr defaultValue);
NodePtr makeAssignment(ParserDriver& driver, const OscadLocation& loc, const OscadLocation& nameLoc, std::string name,
                        NodePtr expr);

NodePtr makeLetOp(ParserDriver& driver, const OscadLocation& loc, NodeList assignments, NodePtr body);
NodePtr makeEchoOp(ParserDriver& driver, const OscadLocation& loc, NodeList arguments, NodePtr body);
NodePtr makeAssertOp(ParserDriver& driver, const OscadLocation& loc, NodeList arguments, NodePtr body);
NodePtr makeFunctionLiteral(ParserDriver& driver, const OscadLocation& loc, NodeList parameters, NodePtr body);

NodePtr makeTernaryOp(ParserDriver& driver, const OscadLocation& loc, NodePtr condition, NodePtr trueExpr,
                       NodePtr falseExpr);
NodePtr makePrimaryCall(ParserDriver& driver, const OscadLocation& loc, NodePtr left, NodeList arguments);
NodePtr makePrimaryIndex(ParserDriver& driver, const OscadLocation& loc, NodePtr left, NodePtr index);
NodePtr makePrimaryMember(ParserDriver& driver, const OscadLocation& loc, NodePtr left, const OscadLocation& memberLoc,
                           std::string member);

template <typename T>
NodePtr makeUnaryOp(ParserDriver& driver, const OscadLocation& loc, NodePtr expr) {
    return std::make_unique<T>(driver.toPosition(loc), nodeCast<Expression>(std::move(expr)));
}

template <typename T>
NodePtr makeBinaryOp(ParserDriver& driver, const OscadLocation& loc, NodePtr left, NodePtr right) {
    return std::make_unique<T>(driver.toPosition(loc), nodeCast<Expression>(std::move(left)),
                                nodeCast<Expression>(std::move(right)));
}

NodePtr makeListCompLet(ParserDriver& driver, const OscadLocation& loc, NodeList assignments, NodePtr body);
NodePtr makeListCompEach(ParserDriver& driver, const OscadLocation& loc, NodePtr body);
NodePtr makeListCompFor(ParserDriver& driver, const OscadLocation& loc, NodeList assignments, NodePtr body);
NodePtr makeListCompCFor(ParserDriver& driver, const OscadLocation& loc, NodeList inits, NodePtr condition,
                          NodeList incrs, NodePtr body);
NodePtr makeListCompIf(ParserDriver& driver, const OscadLocation& loc, NodePtr condition, NodePtr trueExpr);
NodePtr makeListCompIfElse(ParserDriver& driver, const OscadLocation& loc, NodePtr condition, NodePtr trueExpr,
                            NodePtr falseExpr);
NodePtr makeListComprehension(ParserDriver& driver, const OscadLocation& loc, NodeList elements);

NodePtr makeModularCall(ParserDriver& driver, const OscadLocation& loc, const OscadLocation& nameLoc, std::string name,
                         NodeList arguments, NodeList children);
NodePtr makeModularFor(ParserDriver& driver, const OscadLocation& loc, NodeList assignments, NodeList body);
NodePtr makeModularIntersectionFor(ParserDriver& driver, const OscadLocation& loc, NodeList assignments, NodeList body);
NodePtr makeModularLet(ParserDriver& driver, const OscadLocation& loc, NodeList assignments, NodeList children);
NodePtr makeModularEcho(ParserDriver& driver, const OscadLocation& loc, NodeList arguments, NodeList children);
NodePtr makeModularAssert(ParserDriver& driver, const OscadLocation& loc, NodeList arguments, NodeList children);
NodePtr makeModularIf(ParserDriver& driver, const OscadLocation& loc, NodePtr condition, NodeList trueBranch);
NodePtr makeModularIfElse(ParserDriver& driver, const OscadLocation& loc, NodePtr condition, NodeList trueBranch,
                           NodeList falseBranch);

template <typename T>
NodePtr makeModifier(ParserDriver& driver, const OscadLocation& loc, NodePtr child) {
    return std::make_unique<T>(driver.toPosition(loc), nodeCast<ModuleInstantiation>(std::move(child)));
}

NodePtr makeModuleDeclaration(ParserDriver& driver, const OscadLocation& loc, const OscadLocation& nameLoc,
                               std::string name, NodeList parameters, NodeList children);
NodePtr makeFunctionDeclaration(ParserDriver& driver, const OscadLocation& loc, const OscadLocation& nameLoc,
                                 std::string name, NodeList parameters, NodePtr expr);
NodePtr makeUseStatement(ParserDriver& driver, const OscadLocation& loc, std::string path);
NodePtr makeIncludeStatement(ParserDriver& driver, const OscadLocation& loc, std::string path);

} // namespace oscad
