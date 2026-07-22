#include "openscad_cpp_parser/serialization.hpp"

#include <nlohmann/json.hpp>

#include <cctype>
#include <sstream>
#include <stdexcept>

// ponytail: this is a restricted block-style YAML emitter/parser covering
// exactly the shape astToJson()'s nlohmann::json tree produces and
// consumes -- no anchors, no flow style, no multi-document streams, no
// general YAML spec compliance. Ceiling: round-trips our own output only.
// Upgrade path if a user needs to hand-author or ingest arbitrary
// third-party YAML: swap in yaml-cpp (via FetchContent) behind these same
// astToYamlString/astFromYamlString signatures.

namespace oscad {

using json = nlohmann::json;

namespace {

bool isScalarJson(const json& v) {
    return !v.is_object() && !v.is_array();
}

void emitScalar(std::ostringstream& out, const json& value) {
    if (value.is_null()) {
        out << "null";
    } else if (value.is_boolean()) {
        out << (value.get<bool>() ? "true" : "false");
    } else if (value.is_number_integer()) {
        out << value.get<long long>();
    } else if (value.is_number_float()) {
        out << value.get<double>();
    } else if (value.is_string()) {
        const std::string& s = value.get_ref<const std::string&>();
        bool needsQuote = s.empty() || s == "null" || s == "true" || s == "false" ||
                          s.find_first_of(":#\n\"'{}[]&*!|>%@`,") != std::string::npos ||
                          (std::isdigit(static_cast<unsigned char>(s[0])) != 0) || s[0] == '-';
        if (!needsQuote) {
            out << s;
            return;
        }
        out << '"';
        for (char c : s) {
            if (c == '\\' || c == '"') {
                out << '\\' << c;
            } else if (c == '\n') {
                out << "\\n";
            } else {
                out << c;
            }
        }
        out << '"';
    }
}

void emitBlock(std::ostringstream& out, const json& value, int indent) {
    std::string pad(indent, ' ');
    if (value.is_object()) {
        if (value.empty()) {
            out << pad << "{}\n";
            return;
        }
        for (auto it = value.begin(); it != value.end(); ++it) {
            out << pad << it.key() << ":";
            if (isScalarJson(it.value())) {
                out << " ";
                emitScalar(out, it.value());
                out << "\n";
            } else if (it.value().empty()) {
                out << " " << (it.value().is_array() ? "[]" : "{}") << "\n";
            } else {
                out << "\n";
                emitBlock(out, it.value(), indent + 2);
            }
        }
    } else if (value.is_array()) {
        if (value.empty()) {
            out << pad << "[]\n";
            return;
        }
        for (const auto& item : value) {
            out << pad << "-";
            if (isScalarJson(item)) {
                out << " ";
                emitScalar(out, item);
                out << "\n";
            } else if (item.empty()) {
                out << " " << (item.is_array() ? "[]" : "{}") << "\n";
            } else {
                out << "\n";
                emitBlock(out, item, indent + 2);
            }
        }
    }
}

std::string lstrip(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && s[i] == ' ') {
        ++i;
    }
    return s.substr(i);
}

std::string rstrip(const std::string& s) {
    size_t end = s.size();
    while (end > 0 && (s[end - 1] == ' ' || s[end - 1] == '\r')) {
        --end;
    }
    return s.substr(0, end);
}

bool isBlank(const std::string& s) {
    return lstrip(s).empty();
}

int indentOf(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && s[i] == ' ') {
        ++i;
    }
    return static_cast<int>(i);
}

json parseScalar(const std::string& raw) {
    if (raw == "null") {
        return json(nullptr);
    }
    if (raw == "true") {
        return json(true);
    }
    if (raw == "false") {
        return json(false);
    }
    if (!raw.empty() && raw.front() == '"') {
        std::string result;
        for (size_t i = 1; i + 1 < raw.size(); ++i) {
            if (raw[i] == '\\' && i + 1 < raw.size()) {
                char n = raw[i + 1];
                if (n == 'n') {
                    result += '\n';
                } else {
                    result += n;
                }
                ++i;
            } else {
                result += raw[i];
            }
        }
        return json(result);
    }
    try {
        size_t idx = 0;
        double d = std::stod(raw, &idx);
        if (idx == raw.size()) {
            return json(d);
        }
    } catch (const std::exception&) {
        // fall through: plain unquoted string
    }
    return json(raw);
}

class YamlParser {
public:
    explicit YamlParser(std::vector<std::string> lines) : lines_(std::move(lines)) {}

    json parseTop() {
        skipBlank();
        if (pos_ >= lines_.size()) {
            return json::array();
        }
        return parseBlock(indentOf(lines_[pos_]));
    }

private:
    std::vector<std::string> lines_;
    size_t pos_ = 0;

    void skipBlank() {
        while (pos_ < lines_.size() && isBlank(lines_[pos_])) {
            ++pos_;
        }
    }

    json parseInline(const std::string& rest) {
        if (rest == "[]") {
            return json::array();
        }
        if (rest == "{}") {
            return json::object();
        }
        return parseScalar(rest);
    }

    json parseBlock(int indent) {
        skipBlank();
        if (pos_ >= lines_.size() || indentOf(lines_[pos_]) < indent) {
            return json(nullptr);
        }
        std::string content = lstrip(lines_[pos_]);
        bool isArrayItem = content == "-" || (content.size() >= 2 && content[0] == '-' && content[1] == ' ');
        if (isArrayItem) {
            json arr = json::array();
            while (pos_ < lines_.size()) {
                skipBlank();
                if (pos_ >= lines_.size() || indentOf(lines_[pos_]) != indent) {
                    break;
                }
                std::string c = lstrip(lines_[pos_]);
                if (!(c == "-" || (c.size() >= 2 && c[0] == '-' && c[1] == ' '))) {
                    break;
                }
                if (c == "-") {
                    ++pos_;
                    arr.push_back(parseBlock(indent + 2));
                } else {
                    std::string rest = rstrip(c.substr(2));
                    ++pos_;
                    arr.push_back(parseInline(rest));
                }
            }
            return arr;
        }
        json obj = json::object();
        while (pos_ < lines_.size()) {
            skipBlank();
            if (pos_ >= lines_.size() || indentOf(lines_[pos_]) != indent) {
                break;
            }
            std::string c = lstrip(lines_[pos_]);
            size_t colon = c.find(':');
            if (colon == std::string::npos) {
                break;
            }
            std::string key = c.substr(0, colon);
            std::string rest = rstrip(lstrip(c.substr(colon + 1)));
            ++pos_;
            if (rest.empty()) {
                obj[key] = parseBlock(indent + 2);
            } else {
                obj[key] = parseInline(rest);
            }
        }
        return obj;
    }
};

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    size_t start = 0;
    for (size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == '\n') {
            lines.push_back(text.substr(start, i - start));
            start = i + 1;
        }
    }
    return lines;
}

} // namespace

std::string astToYamlString(const std::vector<std::unique_ptr<ASTNode>>& nodes, bool includePosition) {
    json arr = astToJson(nodes, includePosition);
    std::ostringstream out;
    emitBlock(out, arr, 0);
    return out.str();
}

std::vector<std::unique_ptr<ASTNode>> astFromYamlString(const std::string& yamlStr) {
    YamlParser parser(splitLines(yamlStr));
    json data = parser.parseTop();
    return astFromJsonArray(data);
}

} // namespace oscad
