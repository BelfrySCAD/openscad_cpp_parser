#pragma once

#include "openscad_cpp_parser/scope_table.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace oscad {

class ASTNode;

// A lexical scope. OpenSCAD has three separate namespaces -- variables,
// functions, and modules -- so the same name can be bound in all three
// simultaneously. Lookup walks the parent chain; there are no shadowing or
// redefinition diagnostics (last-write-wins), mirroring the Python
// reference implementation exactly.
//
// A Scope does not own the ASTNodes it points to (those are owned by the
// AST tree). A Scope *does* own its child scopes (created via childScope()),
// keeping them alive for as long as the root Scope returned by buildScopes()
// is alive, which is what every node's scope() pointer points into.
class Scope {
public:
    explicit Scope(Scope* parent = nullptr) : parent_(parent) {}

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    Scope(Scope&&) = delete;
    Scope& operator=(Scope&&) = delete;

    // Overloaded on the constness of `this` (rather than a single `const`
    // method returning a raw mutable pointer) so that code holding only a
    // `const Scope&`/`const Scope*` -- e.g. a read-only borrow shared
    // across threads -- gets back `const ASTNode*`/`const Scope*` and
    // can't reach through the result to mutate a declaration node or call
    // defineVariable()/childScope() on a parent scope. A raw pointer's
    // constness doesn't otherwise propagate to what it points to, so
    // without this a "read-only" borrow wouldn't actually be enforced by
    // the compiler.
    const Scope* parent() const { return parent_; }
    Scope* parent() { return parent_; }

    const ASTNode* lookupVariable(const std::string& name) const {
        for (const Scope* s = this; s != nullptr; s = s->parent_) {
            if (auto* n = find(s->variables_, name)) return n;
        }
        return nullptr;
    }
    ASTNode* lookupVariable(const std::string& name) {
        return const_cast<ASTNode*>(static_cast<const Scope*>(this)->lookupVariable(name));
    }

    const ASTNode* lookupFunction(const std::string& name) const {
        for (const Scope* s = this; s != nullptr; s = s->parent_) {
            if (auto* n = find(s->functions_, name)) return n;
        }
        return nullptr;
    }
    ASTNode* lookupFunction(const std::string& name) {
        return const_cast<ASTNode*>(static_cast<const Scope*>(this)->lookupFunction(name));
    }

    const ASTNode* lookupModule(const std::string& name) const {
        for (const Scope* s = this; s != nullptr; s = s->parent_) {
            if (auto* n = find(s->modules_, name)) return n;
        }
        return nullptr;
    }
    ASTNode* lookupModule(const std::string& name) {
        return const_cast<ASTNode*>(static_cast<const Scope*>(this)->lookupModule(name));
    }

    void defineVariable(const std::string& name, ASTNode* node) { variables_[name] = node; }
    void defineFunction(const std::string& name, ASTNode* node) { functions_[name] = node; }
    void defineModule(const std::string& name, ASTNode* node) { modules_[name] = node; }

    // Creates a new scope with `this` as parent. The returned reference is
    // stable for the lifetime of the root Scope: each child is separately
    // heap-allocated (via unique_ptr), so it doesn't move even if
    // `children_` itself reallocates.
    Scope& childScope() {
        children_.push_back(std::make_unique<Scope>(this));
        return *children_.back();
    }

    // The ScopeTable holding every node's scope for this scope tree.
    // buildScopes() attaches it to the root it returns, so the table lives
    // exactly as long as the scopes it points at -- which is the ownership
    // contract callers already keep. Null on a non-root scope; use
    // rootTable() to reach it from anywhere in the tree.
    void adoptTable(std::unique_ptr<ScopeTable> table);
    const ScopeTable* table() const { return ownedTable_.get(); }
    const ScopeTable* rootTable() const {
        const Scope* s = this;
        while (s->parent_) s = s->parent_;
        return s->ownedTable_.get();
    }

private:
    static ASTNode* find(const std::unordered_map<std::string, ASTNode*>& table, const std::string& name) {
        auto it = table.find(name);
        return it != table.end() ? it->second : nullptr;
    }

    Scope* parent_;
    std::unordered_map<std::string, ASTNode*> variables_;
    std::unordered_map<std::string, ASTNode*> functions_;
    std::unordered_map<std::string, ASTNode*> modules_;
    std::vector<std::unique_ptr<Scope>> children_;
    std::unique_ptr<ScopeTable> ownedTable_;   // root only
};

// The lexical Scope recorded for `node` when `anyScopeInTree`'s tree was
// built -- the replacement for the ASTNode::scope() field that used to
// live in the node. Any scope in the tree will do; it walks up to the root
// that owns the table.
inline const Scope* scopeOf(const Scope& anyScopeInTree, const ASTNode& node) {
    const ScopeTable* table = anyScopeInTree.rootTable();
    return table ? table->get(node) : nullptr;
}

} // namespace oscad
