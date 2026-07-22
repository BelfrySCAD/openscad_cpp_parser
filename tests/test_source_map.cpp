#include "openscad_cpp_parser/api.hpp"
#include "openscad_cpp_parser/source_map.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <random>

using namespace oscad;
namespace fs = std::filesystem;

TEST(SourceMap, SingleOriginCombinedStringRoundTrips) {
    SourceMap sm;
    sm.addOrigin("a.scad", "cube(1);\n");
    EXPECT_EQ(sm.getCombinedString(), "cube(1);\n");
}

TEST(SourceMap, TwoOriginsAppendInOrder) {
    SourceMap sm;
    sm.addOrigin("a.scad", "x = 1;\n");
    sm.addOrigin("b.scad", "y = 2;\n");
    EXPECT_EQ(sm.getCombinedString(), "x = 1;\ny = 2;\n");
}

TEST(SourceMap, GetLocationMapsOffsetBackToOrigin) {
    SourceMap sm;
    sm.addOrigin("a.scad", "x = 1;\n");    // offsets 0-7
    sm.addOrigin("b.scad", "y = 2;\n");    // offsets 7-14
    Position pos = sm.getLocation(8);      // 'y' in b.scad's content
    EXPECT_EQ(pos.origin, "b.scad");
    EXPECT_EQ(pos.line, 1);
}

TEST(SourceMap, CreateFromOriginsHelper) {
    auto sm = createSourceMapFromOrigins({{"a.scad", "x = 1;\n"}, {"b.scad", "y = 2;\n"}});
    EXPECT_EQ(sm.getCombinedString(), "x = 1;\ny = 2;\n");
}

namespace {

class TempDir {
public:
    TempDir() : path_(fs::temp_directory_path() / fs::path("oscad_sm_test_" + std::to_string(std::random_device{}()))) {
        fs::create_directories(path_);
    }
    ~TempDir() { std::error_code ec; fs::remove_all(path_, ec); }
    fs::path path() const { return path_; }

private:
    fs::path path_;
};

void writeFile(const fs::path& p, const std::string& content) {
    std::ofstream out(p);
    out << content;
}

} // namespace

TEST(SourceMap, ProcessIncludesSplicesFileContentInPlace) {
    TempDir dir;
    writeFile(dir.path() / "lib.scad", "y = 2;\n");

    SourceMap sm;
    sm.addOrigin((dir.path() / "main.scad").string(), "x = 1;\ninclude <lib.scad>\nz = 3;\n");

    processIncludes(sm, (dir.path() / "main.scad").string());

    // The `include <lib.scad>` directive is replaced by lib.scad's content.
    EXPECT_EQ(sm.getCombinedString(), "x = 1;\ny = 2;\nz = 3;\n");
}

TEST(SourceMap, ProcessIncludesThenParsesCleanly) {
    TempDir dir;
    writeFile(dir.path() / "lib.scad", "module helper() { cube(1); }\n");

    SourceMap sm;
    sm.addOrigin((dir.path() / "main.scad").string(), "include <lib.scad>\nhelper();\n");
    processIncludes(sm, (dir.path() / "main.scad").string());

    auto ast = parseAst(sm.getCombinedString(), (dir.path() / "main.scad").string(), &sm);
    ASSERT_EQ(ast.size(), 2u);
    EXPECT_EQ(ast[0]->kind(), NodeKind::ModuleDeclaration);
    EXPECT_EQ(ast[1]->kind(), NodeKind::ModularCall);
}

TEST(SourceMap, ProcessIncludesThrowsOnMissingFile) {
    SourceMap sm;
    sm.addOrigin("<string>", "include <definitely_missing_xyz.scad>\n");
    EXPECT_THROW(processIncludes(sm, ""), std::runtime_error);
}

TEST(SourceMap, ParseErrorAcrossSplicedFilesReportsCorrectOrigin) {
    TempDir dir;
    // lib.scad has a syntax error inside it.
    writeFile(dir.path() / "lib.scad", "x = ;\n");

    SourceMap sm;
    sm.addOrigin((dir.path() / "main.scad").string(), "include <lib.scad>\n");
    processIncludes(sm, (dir.path() / "main.scad").string());

    try {
        parseAst(sm.getCombinedString(), (dir.path() / "main.scad").string(), &sm);
        FAIL() << "expected ParseError";
    } catch (const ParseError& e) {
        std::string msg = e.what();
        EXPECT_NE(msg.find("lib.scad"), std::string::npos) << "diagnostic should name lib.scad, got: " << msg;
    }
}
