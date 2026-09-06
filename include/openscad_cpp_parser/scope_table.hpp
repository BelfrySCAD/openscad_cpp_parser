#pragma once

#include "openscad_cpp_parser/ast/ast_node.hpp"

#include <cstdint>
#include <vector>

namespace oscad {

class Scope;

// Where a node's Scope lives, now that it cannot live in the node.
//
// A parsed tree is shared: `include <BOSL2/std.scad>` is parsed once and
// then handed to every script that includes it, which is what makes a
// render cheap (parsing BOSL2 is ~55ms and dwarfs everything else). But
// `include` means "share my scope", so the very same BOSL2 node sits in a
// different scope in every script that includes it. A `Scope*` field in
// ASTNode could only ever hold one of them, and whichever render wrote it
// last would corrupt the others.
//
// So the pointer moves out here, into a table one render owns. Addressing
// is (treeId, slot), both stamped on the node when it was parsed, so a
// lookup is two loads and no hashing -- it sits on the VM's call path.
class ScopeTable {
public:
    // Null until set: buildScopes() fills in every node it walks, and a
    // reader treats null as "no scope recorded", exactly as the old
    // nullptr-initialised field did.
    const Scope* get(const ASTNode& node) const {
        const uint32_t tree = node.treeId();
        if (tree >= trees_.size()) return nullptr;
        const std::vector<Scope*>& slots = trees_[tree];
        const uint32_t slot = node.slot();
        return slot < slots.size() ? slots[slot] : nullptr;
    }
    Scope* get(const ASTNode& node) {
        return const_cast<Scope*>(static_cast<const ScopeTable*>(this)->get(node));
    }

    void set(const ASTNode& node, Scope* scope) {
        const uint32_t tree = node.treeId();
        if (tree >= trees_.size()) trees_.resize(tree + 1);
        std::vector<Scope*>& slots = trees_[tree];
        const uint32_t slot = node.slot();
        if (slot >= slots.size()) slots.resize(slot + 1, nullptr);
        slots[slot] = scope;
    }

private:
    // Indexed by treeId. Sparse in principle -- treeIds are handed out
    // process-wide and a render only involves a few trees -- but the empty
    // outer entries are 24 bytes each and never allocate, so the waste is
    // a few tens of KB against a 55ms saving.
    std::vector<std::vector<Scope*>> trees_;
};

} // namespace oscad
