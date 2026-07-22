#include "openscad_cpp_parser/api.hpp"

#include <gtest/gtest.h>

#include <type_traits>

using namespace oscad;

namespace {

// Exercises the "one owner, many read-only borrowers" pattern: a function
// that only ever sees `const ASTNode&`/`const Scope*` must still be able to
// read scope/lookup info, and the types it gets back must themselves be
// const -- so there's no back door to mutation through a "read-only" borrow.
const ASTNode* readOnlyLookup(const ASTNode& borrowed, const std::string& name) {
    const Scope* s = borrowed.scope();
    static_assert(std::is_same_v<decltype(s), const Scope*>, "scope() through a const ASTNode& must yield const Scope*");
    if (!s) {
        return nullptr;
    }
    const ASTNode* found = s->lookupVariable(name);
    static_assert(std::is_same_v<decltype(found), const ASTNode*>,
                  "lookupVariable() through a const Scope* must yield const ASTNode*");
    return found;
}

} // namespace

TEST(ConstBorrow, ScopeAccessorsAreConstCorrect) {
    // Non-const access still yields mutable pointers (existing behavior,
    // e.g. buildScopes()/collectHoistedDeclarations() need to mutate).
    static_assert(std::is_same_v<decltype(std::declval<ASTNode&>().scope()), Scope*>);
    static_assert(std::is_same_v<decltype(std::declval<const ASTNode&>().scope()), const Scope*>);
    static_assert(std::is_same_v<decltype(std::declval<Scope&>().lookupVariable("x")), ASTNode*>);
    static_assert(std::is_same_v<decltype(std::declval<const Scope&>().lookupVariable("x")), const ASTNode*>);
    static_assert(std::is_same_v<decltype(std::declval<Scope&>().lookupFunction("x")), ASTNode*>);
    static_assert(std::is_same_v<decltype(std::declval<const Scope&>().lookupFunction("x")), const ASTNode*>);
    static_assert(std::is_same_v<decltype(std::declval<Scope&>().lookupModule("x")), ASTNode*>);
    static_assert(std::is_same_v<decltype(std::declval<const Scope&>().lookupModule("x")), const ASTNode*>);
    static_assert(std::is_same_v<decltype(std::declval<Scope&>().parent()), Scope*>);
    static_assert(std::is_same_v<decltype(std::declval<const Scope&>().parent()), const Scope*>);
    SUCCEED();
}

TEST(ConstBorrow, ReadOnlyLookupWorksThroughConstReferences) {
    auto ast = getASTFromString("y = 10;\nfunction f(a = y) = a;");
    auto root = buildScopes(ast);

    auto* func = dynamic_cast<FunctionDeclaration*>(ast[1].get());
    ASSERT_NE(func, nullptr);
    const Expression& borrowedDefault = *func->parameters[0]->defaultValue; // const borrow, as a worker thread would see it

    const ASTNode* resolved = readOnlyLookup(borrowedDefault, "y");
    ASSERT_NE(resolved, nullptr);
    EXPECT_EQ(resolved->kind(), NodeKind::Assignment);
}

// Compile-only check: verifying a mutating call is rejected when only a
// const borrow is available would need a "must fail to compile" test
// harness this project doesn't have, so it's asserted here in prose
// instead -- `borrowedDefault.buildScope(*root)` and
// `borrowedDefault.scope()->defineVariable(...)` both fail to compile if
// uncommented (try it by hand if in doubt), since scope() on a const
// object now yields `const Scope*`. Like all const-correctness in C++,
// this is a static convention, not a runtime guarantee: an explicit
// `const_cast<Scope&>(*borrowedDefault.scope()).defineVariable(...)` still
// compiles and is well-defined (the underlying Scope was never actually
// const, only const-accessed) -- the fix makes accidental mutation a
// compile error, not a determined bypass impossible.
