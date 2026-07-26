#include "openscad_cpp_parser/api.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <random>

using namespace oscad;
namespace fs = std::filesystem;

TEST(Comments, StandaloneCommentInjectedAtTopLevel) {
    auto ast = getASTFromString("// header comment\nx = 1;\n", /*includeComments=*/true);
    ASSERT_EQ(ast.size(), 2u);
    EXPECT_EQ(ast[0]->kind(), NodeKind::CommentLine);
    EXPECT_EQ(ast[1]->kind(), NodeKind::Assignment);
}

TEST(Comments, BlankLinePreservedBetweenComments) {
    auto ast = getASTFromString("// first\n\n// second\nx = 1;\n", /*includeComments=*/true);
    // CommentLine, BlankLine, CommentLine, Assignment
    ASSERT_EQ(ast.size(), 4u);
    EXPECT_EQ(ast[0]->kind(), NodeKind::CommentLine);
    EXPECT_EQ(ast[1]->kind(), NodeKind::BlankLine);
    EXPECT_EQ(ast[2]->kind(), NodeKind::CommentLine);
    EXPECT_EQ(ast[3]->kind(), NodeKind::Assignment);
}

TEST(Comments, NoCommentsWithoutFlag) {
    auto ast = getASTFromString("// header comment\nx = 1;\n", /*includeComments=*/false);
    ASSERT_EQ(ast.size(), 1u);
    EXPECT_EQ(ast[0]->kind(), NodeKind::Assignment);
}

TEST(Comments, StringLiteralCommentMarkersNotTreatedAsComments) {
    auto ast = getASTFromString("x = \"// not a comment\";\n", /*includeComments=*/true);
    ASSERT_EQ(ast.size(), 1u);
    auto* a = dynamic_cast<Assignment*>(ast[0].get());
    ASSERT_NE(a, nullptr);
    auto* str = dynamic_cast<StringLiteral*>(a->expr.get());
    ASSERT_NE(str, nullptr);
    EXPECT_EQ(str->val, "// not a comment");
}

namespace {

class TempDir {
public:
    TempDir() : path_(fs::temp_directory_path() / fs::path("oscad_test_" + std::to_string(std::random_device{}()))) {
        fs::create_directories(path_);
    }
    ~TempDir() { std::error_code ec; fs::remove_all(path_, ec); }
    fs::path path() const { return path_; }

private:
    fs::path path_;
};

void writeFile(const fs::path& p, const std::string& content) {
    // Binary mode: see the identical comment in test_source_map.cpp's own
    // writeFile -- avoids Windows text-mode \r\n injection that the
    // production reader (binary mode) would then faithfully preserve.
    std::ofstream out(p, std::ios::binary);
    out << content;
}

} // namespace

TEST(FileApi, GetASTFromFileParsesSimpleFile) {
    TempDir dir;
    fs::path file = dir.path() / "model.scad";
    writeFile(file, "cube([1,2,3]);\n");

    auto ast = getASTFromFile(file.string());
    ASSERT_EQ(ast.size(), 1u);
    EXPECT_EQ(ast[0]->kind(), NodeKind::ModularCall);
}

TEST(FileApi, GetASTFromFileThrowsOnMissingFile) {
    EXPECT_THROW(getASTFromFile("/nonexistent/path/does_not_exist.scad"), std::runtime_error);
}

TEST(FileApi, IncludeStatementIsSplicedIn) {
    TempDir dir;
    fs::path libFile = dir.path() / "lib.scad";
    writeFile(libFile, "module helper() { cube(1); }\n");

    fs::path mainFile = dir.path() / "main.scad";
    writeFile(mainFile, "include <lib.scad>\nhelper();\n");

    auto ast = getASTFromFile(mainFile.string());
    // The IncludeStatement is replaced by lib.scad's own top-level nodes.
    ASSERT_EQ(ast.size(), 2u);
    EXPECT_EQ(ast[0]->kind(), NodeKind::ModuleDeclaration);
    EXPECT_EQ(ast[1]->kind(), NodeKind::ModularCall);
}

TEST(FileApi, UseStatementIsNotResolved) {
    TempDir dir;
    fs::path libFile = dir.path() / "lib.scad";
    writeFile(libFile, "module helper() { cube(1); }\n");

    fs::path mainFile = dir.path() / "main.scad";
    writeFile(mainFile, "use <lib.scad>\nhelper();\n");

    auto ast = getASTFromFile(mainFile.string());
    ASSERT_EQ(ast.size(), 2u);
    EXPECT_EQ(ast[0]->kind(), NodeKind::UseStatement);
    EXPECT_EQ(ast[1]->kind(), NodeKind::ModularCall);
}

TEST(FileApi, IncludeCycleDoesNotInfiniteLoop) {
    TempDir dir;
    fs::path a = dir.path() / "a.scad";
    fs::path b = dir.path() / "b.scad";
    writeFile(a, "include <b.scad>\nx = 1;\n");
    writeFile(b, "include <a.scad>\ny = 2;\n");

    auto ast = getASTFromFile(a.string());
    // a's own include of b is followed once; b's include of a is a
    // revisit and skipped, so we get b's y=2 then a's x=1.
    ASSERT_EQ(ast.size(), 2u);
}

TEST(FileApi, FindLibraryFileSearchesCurrentFileDirectory) {
    TempDir dir;
    fs::path libFile = dir.path() / "helper.scad";
    writeFile(libFile, "x = 1;\n");
    fs::path mainFile = dir.path() / "main.scad";
    writeFile(mainFile, "x = 1;\n");

    auto found = findLibraryFile(mainFile.string(), "helper.scad");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(fs::path(*found), libFile);
}

TEST(FileApi, FindLibraryFileReturnsNulloptWhenMissing) {
    auto found = findLibraryFile("", "definitely_missing_file_xyz.scad");
    EXPECT_FALSE(found.has_value());
}
