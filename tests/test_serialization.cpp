#include "openscad_cpp_parser/api.hpp"
#include "openscad_cpp_parser/pretty_print.hpp"
#include "openscad_cpp_parser/serialization.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using namespace oscad;

namespace {

// Structural round-trip proxy: since C++ has no dataclass-style structural
// equality, compare the pretty-printed text of the original AST against
// the pretty-printed text of the AST rebuilt from JSON/YAML. If
// serialization dropped or corrupted a field, formatting will differ.
void expectJsonRoundTrip(const std::string& src) {
    auto ast = parseAst(src);
    std::string before = toOpenscad(ast);
    std::string jsonStr = astToJsonString(ast);
    auto rebuilt = astFromJsonString(jsonStr);
    std::string after = toOpenscad(rebuilt);
    EXPECT_EQ(before, after) << "JSON round-trip mismatch for source:\n" << src;
}

void expectYamlRoundTrip(const std::string& src) {
    auto ast = parseAst(src);
    std::string before = toOpenscad(ast);
    std::string yamlStr = astToYamlString(ast);
    auto rebuilt = astFromYamlString(yamlStr);
    std::string after = toOpenscad(rebuilt);
    EXPECT_EQ(before, after) << "YAML round-trip mismatch for source:\n" << src;
}

} // namespace

TEST(Serialization, JsonRoundTripSimple) {
    expectJsonRoundTrip("x = 42;");
}

TEST(Serialization, JsonRoundTripModuleAndFunction) {
    expectJsonRoundTrip("module box(w, h=2) { cube([w, h, 1]); }\nfunction add(a, b) = a + b;");
}

TEST(Serialization, JsonRoundTripControlStructures) {
    expectJsonRoundTrip("if (true) { cube(1); } else { sphere(1); }\nfor (i = [0:5]) cube(i);");
}

TEST(Serialization, JsonRoundTripListComprehension) {
    expectJsonRoundTrip("x = [for (i = [0:5]) if (i % 2 == 0) i];");
}

TEST(Serialization, JsonRoundTripLetAndModifiers) {
    expectJsonRoundTrip("x = let(a = 1, b = a + 1) a + b;\n#!cube(1);");
}

TEST(Serialization, JsonRoundTripUseInclude) {
    expectJsonRoundTrip("use <shapes.scad>\ninclude <lib.scad>\nx = 1;");
}

TEST(Serialization, JsonPreservesNumericValue) {
    auto ast = parseAst("x = 3.5;");
    auto j = astToJson(ast);
    auto rebuilt = astFromJsonArray(j);
    auto* a = dynamic_cast<Assignment*>(rebuilt[0].get());
    ASSERT_NE(a, nullptr);
    auto* num = dynamic_cast<NumberLiteral*>(a->expr.get());
    ASSERT_NE(num, nullptr);
    EXPECT_DOUBLE_EQ(num->val, 3.5);
}

TEST(Serialization, YamlRoundTripSimple) {
    expectYamlRoundTrip("x = 42;");
}

TEST(Serialization, YamlRoundTripComplex) {
    expectYamlRoundTrip(
        "module shelf(width=60, depth=30) {\n"
        "    cube([width, depth, 3]);\n"
        "}\n"
        "shelf(width=80);\n");
}

TEST(Serialization, YamlRoundTripStringEscaping) {
    expectYamlRoundTrip("x = \"hello: world, \\\"quoted\\\"\";");
}
