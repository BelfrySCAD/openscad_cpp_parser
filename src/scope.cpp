#include "openscad_cpp_parser/api.hpp"

#include <atomic>

#include "openscad_cpp_parser/scope_table.hpp"

#include "openscad_cpp_parser/ast/scope_builder.hpp"

namespace oscad {

std::unique_ptr<Scope> buildScopes(const std::vector<std::unique_ptr<ASTNode>>& ast) {
    auto owned = std::make_unique<ScopeTable>();
    ScopeTableScope recording(*owned);
    auto root = std::make_unique<Scope>();
    collectHoistedDeclarations(ast, *root);
    for (auto& node : ast) {
        node->buildScope(*root);
    }
    root->adoptTable(std::move(owned));
    return root;
}

std::unique_ptr<Scope> buildScopes(const std::vector<ASTNode*>& ast) {
    auto owned = std::make_unique<ScopeTable>();
    ScopeTableScope recording(*owned);
    auto root = std::make_unique<Scope>();
    collectHoistedDeclarations(ast, *root);
    for (ASTNode* node : ast) {
        node->buildScope(*root);
    }
    root->adoptTable(std::move(owned));
    return root;
}


namespace {
// One per thread: a ParseNumberingScope installs a numbering for its
// duration, so a node's constructor can stamp itself without every node
// kind having to cooperate -- which matters because there is no generic
// child walker to number a finished tree with.
thread_local NodeNumbering* g_numbering = nullptr;
thread_local ScopeTable* g_scopeTable = nullptr;
std::atomic<uint32_t> g_nextTreeId{1};   // 0 is reserved for loose nodes
std::atomic<uint32_t> g_nextLooseSlot{0};
} // namespace

NodeNumbering* currentNodeNumbering() { return g_numbering; }
uint32_t nextLooseSlot() { return g_nextLooseSlot.fetch_add(1, std::memory_order_relaxed); }

ParseNumberingScope::ParseNumberingScope()
    : numbering_{g_nextTreeId.fetch_add(1, std::memory_order_relaxed), 0},
      previous_(g_numbering),
      installed_(g_numbering == nullptr) {
    // Only the outermost scope installs: a nested one keeps the outer
    // numbering so a parse plus its comment-attach pass stay one tree.
    if (installed_) g_numbering = &numbering_;
}

ParseNumberingScope::~ParseNumberingScope() {
    if (installed_) g_numbering = previous_;
}

ScopeTableScope::ScopeTableScope(ScopeTable& table) : previous_(g_scopeTable) { g_scopeTable = &table; }
ScopeTableScope::~ScopeTableScope() { g_scopeTable = previous_; }

void Scope::adoptTable(std::unique_ptr<ScopeTable> table) { ownedTable_ = std::move(table); }

void ASTNode::setScope(Scope& s) {
    // No table installed means nobody is recording scopes -- a parser-only
    // caller walking a tree for its own reasons. Dropping the write keeps
    // that a no-op rather than a crash.
    if (g_scopeTable) g_scopeTable->set(*this, &s);
}

} // namespace oscad
