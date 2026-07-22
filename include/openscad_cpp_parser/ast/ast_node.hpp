#pragma once

#include "openscad_cpp_parser/position.hpp"

#include <string>

namespace oscad {

class Scope;

// One value per instantiable (leaf) AST node class. Abstract bases
// (Expression, Primary, Argument, VectorElement, ModuleInstantiation) have
// no NodeKind of their own, since they are never directly constructed.
enum class NodeKind {
    // Comments
    CommentLine,
    BlankLine,
    CommentSpan,
    CommentedExpr,
    // Primary / literals
    Identifier,
    StringLiteral,
    NumberLiteral,
    BooleanLiteral,
    UndefinedLiteral,
    RangeLiteral,
    // Declarations / arguments
    ParameterDeclaration,
    PositionalArgument,
    NamedArgument,
    Assignment,
    // Prefix expression forms
    LetOp,
    EchoOp,
    AssertOp,
    FunctionLiteral,
    // Operators
    UnaryMinusOp,
    AdditionOp,
    SubtractionOp,
    MultiplicationOp,
    DivisionOp,
    ModuloOp,
    ExponentOp,
    BitwiseAndOp,
    BitwiseOrOp,
    BitwiseNotOp,
    BitwiseShiftLeftOp,
    BitwiseShiftRightOp,
    LogicalAndOp,
    LogicalOrOp,
    LogicalNotOp,
    TernaryOp,
    EqualityOp,
    InequalityOp,
    GreaterThanOp,
    GreaterThanOrEqualOp,
    LessThanOp,
    LessThanOrEqualOp,
    // Postfix
    PrimaryCall,
    PrimaryIndex,
    PrimaryMember,
    // List comprehension
    ListCompLet,
    ListCompEach,
    ListCompFor,
    ListCompCFor,
    ListCompIf,
    ListCompIfElse,
    ListComprehension,
    // Module instantiation
    ModularCall,
    ModularFor,
    ModularIntersectionFor,
    ModularLet,
    ModularEcho,
    ModularAssert,
    ModularIf,
    ModularIfElse,
    ModularModifierShowOnly,
    ModularModifierHighlight,
    ModularModifierBackground,
    ModularModifierDisable,
    // Top-level declarations
    ModuleDeclaration,
    FunctionDeclaration,
    UseStatement,
    IncludeStatement,
};

const char* nodeKindName(NodeKind kind);

// Base class for all AST nodes. Mirrors openscad_lalr_parser.nodes.ASTNode:
// every node carries its source Position and (once buildScope() has run) a
// non-owning pointer to the Scope visible at that point in the tree.
class ASTNode {
public:
    ASTNode(NodeKind kind, Position position) : kind_(kind), position_(std::move(position)) {}
    virtual ~ASTNode() = default;

    ASTNode(const ASTNode&) = delete;
    ASTNode& operator=(const ASTNode&) = delete;

    NodeKind kind() const { return kind_; }
    const Position& position() const { return position_; }

    // Overloaded on the constness of `this` rather than a single `const`
    // method returning `Scope*` unconditionally: a plain raw-pointer
    // return type doesn't propagate constness to the pointee, so a caller
    // holding only `const ASTNode&` (e.g. a read-only borrow shared across
    // threads while one owner keeps the tree alive) could otherwise still
    // reach through scope() and call a mutating method like
    // defineVariable() on the Scope -- defeating the whole point of the
    // borrow being const. This makes that a compile error instead.
    Scope* scope() { return scope_; }
    const Scope* scope() const { return scope_; }

    // Mirrors Python's __str__: every leaf node overrides this.
    virtual std::string toString() const = 0;

    // Mirrors Python's build_scope(parent_scope): the default (leaf) case
    // just records parent_scope; nodes that introduce bindings or new
    // scopes override this.
    virtual void buildScope(Scope& parentScope) { scope_ = &parentScope; }

protected:
    // For buildScope() overrides: records the scope visible at this node,
    // mirroring Python's `self.scope = parent_scope` (which is often a
    // *different* scope than what gets passed to this node's children).
    void setScope(Scope& s) { scope_ = &s; }

private:
    NodeKind kind_;
    Position position_;
    Scope* scope_ = nullptr;
};

} // namespace oscad
