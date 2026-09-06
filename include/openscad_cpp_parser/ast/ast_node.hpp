#pragma once

#include "openscad_cpp_parser/position.hpp"

#include <cstdint>
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
    // render() in EXPRESSION position -- evaluates its children as geometry
    // and yields an object() of measurements. The STATEMENT form of render()
    // is a plain ModularCall named "render"; only this one is new.
    RenderExpression,
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
// Numbering context for the nodes one parse creates: every node built
// while it is installed gets that parse's treeId and a slot dense within
// it -- see ASTNode::slot().
struct NodeNumbering {
    uint32_t treeId = 0;
    uint32_t next = 0;
};

// Installs a numbering for a whole parse, INCLUDING any pass that builds
// more nodes from its result (attachComments wraps expressions in
// CommentedExpr nodes after the parser proper has finished). Nested use
// reuses the outer numbering rather than starting a second one, so a tree
// stays a tree however many passes contribute to it.
//
// Thread-local: two threads may parse at once and must number
// independently.
class ParseNumberingScope {
public:
    ParseNumberingScope();
    ~ParseNumberingScope();
    ParseNumberingScope(const ParseNumberingScope&) = delete;
    ParseNumberingScope& operator=(const ParseNumberingScope&) = delete;

private:
    NodeNumbering numbering_;
    NodeNumbering* previous_;
    bool installed_;
};

class ScopeTable;

// The table setScope() writes into. buildScopes() installs one for its
// walk; thread-local, so two evaluations can build scopes at once over the
// same shared tree -- the whole point of sharing it.
class ScopeTableScope {
public:
    explicit ScopeTableScope(ScopeTable& table);
    ~ScopeTableScope();
    ScopeTableScope(const ScopeTableScope&) = delete;
    ScopeTableScope& operator=(const ScopeTableScope&) = delete;

private:
    ScopeTable* previous_;
};

NodeNumbering* currentNodeNumbering();
// Slot for a node built with no parse active -- a test constructing nodes
// by hand, or astFromJson. Those all land in treeId 0 with process-unique
// slots, so they never collide with each other.
uint32_t nextLooseSlot();

class ASTNode {
public:
    ASTNode(NodeKind kind, Position position) : kind_(kind), position_(std::move(position)) {
        if (NodeNumbering* n = currentNodeNumbering()) {
            treeId_ = n->treeId;
            slot_ = n->next++;
        } else {
            slot_ = nextLooseSlot();
        }
    }
    virtual ~ASTNode() = default;

    ASTNode(const ASTNode&) = delete;
    ASTNode& operator=(const ASTNode&) = delete;

    NodeKind kind() const { return kind_; }
    const Position& position() const { return position_; }

    // Identity of the parse that built this node, and this node's dense
    // index within it. Together they address the node's Scope in a
    // ScopeTable, which is what lets one parsed tree be shared by several
    // evaluations at once: the scope a node sits in depends on the file
    // that included it, so it cannot live in the node itself. Both are 0
    // for a node built outside a parse (a test constructing nodes by
    // hand), which is a valid single tree of its own.
    uint32_t treeId() const { return treeId_; }
    uint32_t slot() const { return slot_; }

    // A node's Scope is NOT stored here -- it lives in the render's
    // ScopeTable, addressed by (treeId, slot). It has to: one parsed tree
    // is shared by every script that includes it, and `include` means the
    // included nodes sit in the INCLUDER's scope, so the same node is in a
    // different scope in each. Read it with ScopeTable::get(node).

    // Mirrors Python's __str__: every leaf node overrides this.
    virtual std::string toString() const = 0;

    // Mirrors Python's build_scope(parent_scope): the default (leaf) case
    // just records parent_scope; nodes that introduce bindings or new
    // scopes override this.
    virtual void buildScope(Scope& parentScope) { setScope(parentScope); }

protected:
    // For buildScope() overrides: records the scope visible at this node,
    // mirroring Python's `self.scope = parent_scope` (which is often a
    // *different* scope than what gets passed to this node's children).
    //
    // Writes into the ScopeTable buildScopes() installed for the duration
    // of its walk, rather than taking one as a parameter: that keeps all
    // 37 buildScope() overrides on their existing signature, and the walk
    // is a single scoped pass with nothing else running inside it.
    void setScope(Scope& s);

private:
    NodeKind kind_;
    Position position_;
    uint32_t treeId_ = 0;
    uint32_t slot_ = 0;
};

} // namespace oscad
