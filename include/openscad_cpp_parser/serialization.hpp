#pragma once

#include "openscad_cpp_parser/ast.hpp"

#include <nlohmann/json_fwd.hpp>

#include <memory>
#include <string>
#include <vector>

namespace oscad {

// Structural JSON serialization of an AST tree. No reflection in C++, so
// each node kind is handled explicitly (see serialization/json_io.cpp) --
// but the schema mirrors serialization.py's: `{"_type": "ClassName",
// "_position": {...}?, <field>: <value>, ...}`, using the same field names
// as the Python reference (translated from snake_case to the same
// snake_case JSON keys, independent of this port's camelCase C++ member
// names) so JSON produced by either library is structurally comparable.
nlohmann::json astToJson(const ASTNode& node, bool includePosition = true);
nlohmann::json astToJson(const std::vector<std::unique_ptr<ASTNode>>& nodes, bool includePosition = true);

std::string astToJsonString(const std::vector<std::unique_ptr<ASTNode>>& nodes, bool includePosition = true, int indent = 2);

std::unique_ptr<ASTNode> astFromJson(const nlohmann::json& data);
std::vector<std::unique_ptr<ASTNode>> astFromJsonArray(const nlohmann::json& data);
std::vector<std::unique_ptr<ASTNode>> astFromJsonString(const std::string& jsonStr);

// Restricted YAML: block-style mapping/sequence/scalar emitter and parser
// covering exactly the shape astToJson() produces (no anchors, no flow
// style, no multi-document streams -- see yaml_io.cpp).
std::string astToYamlString(const std::vector<std::unique_ptr<ASTNode>>& nodes, bool includePosition = true);
std::vector<std::unique_ptr<ASTNode>> astFromYamlString(const std::string& yamlStr);

} // namespace oscad
