// The flex scanner is non-reentrant, global state (grammar/lexer_api.hpp),
// so two threads parsing at once used to lex each other's input. In
// BelfrySCAD that was an intermittent 0xC0000005 on Windows: a render worker
// parsing its file while the UI thread parsed the buffer for the Customizer.
// parseAst now serialises the scanner. Every thread here parses its OWN
// source and checks it got its own tree back, so corruption fails the test
// even where it does not crash.
#include "openscad_cpp_parser/api.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

using namespace oscad;

namespace {
std::string sourceFor(int thread, int round) {
    // Different identifiers and lengths per thread, so a lexer reading
    // another thread's buffer produces a different tree, not an equal one.
    std::string name = "v" + std::to_string(thread) + "_" + std::to_string(round);
    std::string src;
    for (int i = 0; i < 20 + thread; ++i) {
        src += name + "_" + std::to_string(i) + " = [" + std::to_string(i) + ", " +
               std::to_string(thread) + ", \"s" + std::to_string(round) + "\"];\n";
    }
    return src;
}
} // namespace

TEST(ConcurrentParse, ThreadsParsingAtOnceEachGetTheirOwnTree) {
    constexpr int kThreads = 8, kRounds = 150;
    std::atomic<int> wrong{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([t, &wrong] {
            for (int r = 0; r < kRounds; ++r) {
                const std::string src = sourceFor(t, r);
                try {
                    auto ast = parseAst(src);
                    const auto* first = ast.empty() ? nullptr : dynamic_cast<const Assignment*>(ast[0].get());
                    const std::string want = "v" + std::to_string(t) + "_" + std::to_string(r) + "_0";
                    if (ast.size() != static_cast<size_t>(20 + t) || !first || first->name->name != want) {
                        ++wrong;
                    }
                } catch (...) {
                    ++wrong;   // a syntax error from lexing another thread's input
                }
            }
        });
    }
    for (auto& th : threads) th.join();
    EXPECT_EQ(wrong.load(), 0);
}
