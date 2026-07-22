#include "openscad_cpp_parser/api.hpp"

#include "grammar/driver.hpp"
#include "grammar/lexer_api.hpp"
#include "openscad_cpp_parser/comments.hpp"
#include "parser.tab.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

namespace oscad {

namespace fs = std::filesystem;

namespace {

std::string expandTabs(const std::string& s, int tabSize = 8) {
    std::string result;
    int col = 0;
    for (char c : s) {
        if (c == '\t') {
            int spaces = tabSize - (col % tabSize);
            result.append(static_cast<size_t>(spaces), ' ');
            col += spaces;
        } else {
            result += c;
            col += 1;
        }
    }
    return result;
}

std::vector<std::string> splitLinesKeepEmpty(const std::string& text) {
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

// Mirrors parse_ast's diagnostic block in __init__.py: "Syntax error in
// {origin} at line L, column C:" followed by the offending source line and
// a caret under the error column (tab-expanded to line up visually).
std::string formatSyntaxError(const ParserDriver& driver, const std::string& code, const std::string& origin,
                               SourceMap* sourceMap) {
    std::string errorOrigin = origin;
    int errorLine = driver.errorLine;
    int errorColumn = driver.errorColumn;
    std::string combinedCode = code;

    if (sourceMap != nullptr) {
        Position loc = sourceMap->getLocation(driver.errorOffset);
        errorOrigin = loc.origin;
        errorLine = loc.line;
        errorColumn = loc.column;
        combinedCode = sourceMap->getCombinedString();
    }

    std::ostringstream out;
    out << "Syntax error in " << errorOrigin << " at line " << errorLine << ", column " << errorColumn << ":";

    std::vector<std::string> lines = splitLinesKeepEmpty(combinedCode);
    if (errorLine >= 1 && static_cast<size_t>(errorLine) <= lines.size()) {
        const std::string& lineText = lines[static_cast<size_t>(errorLine) - 1];
        out << "\n" << lineText;
        int caretPos = std::max(0, errorColumn - 1);
        if (caretPos > static_cast<int>(lineText.size())) {
            caretPos = static_cast<int>(lineText.size());
        }
        std::string expanded = expandTabs(lineText.substr(0, static_cast<size_t>(caretPos)));
        out << "\n" << std::string(expanded.size(), ' ') << "^";
    }
    if (!driver.errorReason.empty()) {
        out << "\n" << driver.errorReason;
    }
    return out.str();
}

} // namespace

std::vector<std::unique_ptr<ASTNode>> parseAst(const std::string& code, const std::string& origin, SourceMap* sourceMap) {
    ParserDriver driver(origin);
    lexerBeginString(code);
    yy::parser parser(driver);
    int rc = parser.parse();
    lexerEnd();

    if (rc != 0 || driver.hadError) {
        throw ParseError(formatSyntaxError(driver, code, origin, sourceMap));
    }
    return std::move(driver.result);
}

std::vector<std::unique_ptr<ASTNode>> getASTFromString(const std::string& code, bool includeComments,
                                                        const std::string& origin) {
    auto ast = parseAst(code, origin); // propagates ParseError with the full diagnostic
    if (includeComments) {
        ast = attachComments(std::move(ast), code, origin);
    }
    return ast;
}

namespace {

std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("File " + path + " not found");
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::vector<std::unique_ptr<ASTNode>> parseSingleFile(const std::string& filePath, bool includeComments) {
    std::string code = readFile(filePath);
    auto ast = parseAst(code, filePath);
    if (includeComments) {
        ast = attachComments(std::move(ast), code, filePath);
    }
    return ast;
}

std::vector<std::unique_ptr<ASTNode>> resolveIncludes(std::vector<std::unique_ptr<ASTNode>> astNodes,
                                                        const std::string& currentFile, bool includeComments,
                                                        std::set<std::string>& visited) {
    std::vector<std::unique_ptr<ASTNode>> result;
    for (auto& node : astNodes) {
        if (node->kind() == NodeKind::IncludeStatement) {
            const auto& inc = static_cast<const IncludeStatement&>(*node);
            const std::string& filename = inc.filepath->val;
            auto libFile = findLibraryFile(currentFile, filename);
            if (!libFile) {
                throw std::runtime_error("Included file '" + filename + "' not found. Searched relative to: " +
                                          (currentFile.empty() ? "current directory" : currentFile));
            }
            std::string absLib = fs::absolute(*libFile).string();
            if (visited.count(absLib) != 0) {
                continue;
            }
            visited.insert(absLib);
            auto includedAst = parseSingleFile(absLib, includeComments);
            includedAst = resolveIncludes(std::move(includedAst), absLib, includeComments, visited);
            for (auto& n : includedAst) {
                result.push_back(std::move(n));
            }
        } else {
            result.push_back(std::move(node));
        }
    }
    return result;
}

} // namespace

std::optional<std::string> findLibraryFile(const std::string& currFile, const std::string& libFile) {
    std::vector<fs::path> dirs;
    if (!currFile.empty()) {
        dirs.push_back(fs::absolute(currFile).parent_path());
    }

    char pathsep = ':';
    std::string dfltPath;
    const char* home = std::getenv("HOME");
#if defined(_WIN32)
    pathsep = ';';
    const char* userProfile = std::getenv("USERPROFILE");
    if (userProfile) {
        dfltPath = std::string(userProfile) + "\\Documents\\OpenSCAD\\libraries";
    }
#elif defined(__APPLE__)
    if (home) {
        dfltPath = std::string(home) + "/Documents/OpenSCAD/libraries";
    }
#else
    if (home) {
        dfltPath = std::string(home) + "/.local/share/OpenSCAD/libraries";
    }
#endif

    const char* envPath = std::getenv("OPENSCADPATH");
    std::string env = envPath ? std::string(envPath) : dfltPath;
    if (!env.empty()) {
        size_t start = 0;
        while (start <= env.size()) {
            size_t pos = env.find(pathsep, start);
            std::string part = (pos == std::string::npos) ? env.substr(start) : env.substr(start, pos - start);
            if (!part.empty()) {
                dirs.emplace_back(part);
            }
            if (pos == std::string::npos) {
                break;
            }
            start = pos + 1;
        }
    }

    for (const auto& d : dirs) {
        fs::path candidate = d / libFile;
        std::error_code ec;
        if (fs::is_regular_file(candidate, ec)) {
            return candidate.string();
        }
    }
    return std::nullopt;
}

std::vector<std::unique_ptr<ASTNode>> getASTFromFile(const std::string& file, bool includeComments,
                                                      bool processIncludes) {
    fs::path filePath = fs::absolute(file);
    if (!fs::exists(filePath)) {
        throw std::runtime_error("File " + file + " not found");
    }
    auto ast = parseSingleFile(filePath.string(), includeComments); // propagates ParseError
    if (processIncludes) {
        std::set<std::string> visited{filePath.string()};
        ast = resolveIncludes(std::move(ast), filePath.string(), includeComments, visited);
    }
    return ast;
}

LibraryFileResult getASTFromLibraryFile(const std::string& currFile, const std::string& libFile, bool includeComments,
                                         bool processIncludes) {
    auto found = findLibraryFile(currFile, libFile);
    if (!found) {
        throw std::runtime_error("Library file '" + libFile +
                                  "' not found in search paths. Searched in: current file directory, OPENSCADPATH, and "
                                  "platform default paths.");
    }
    auto ast = getASTFromFile(*found, includeComments, processIncludes);
    return LibraryFileResult{std::move(ast), *found};
}

void clearAstCache() {
    // No-op -- see the ponytail note on getASTFromFile() in api.hpp.
}

} // namespace oscad
