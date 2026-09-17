#include "openscad_cpp_parser/api.hpp"

#include "grammar/driver.hpp"
#include "grammar/lexer_api.hpp"
#include "openscad_cpp_parser/comments.hpp"
#include "parser.tab.hpp"

#include <cstdlib>
#include <filesystem>
#if defined(_WIN32)
#include <windows.h>
#include <shlobj.h>
#else
#include <dlfcn.h>
#endif
#include <fstream>
#include <set>
#include <system_error>
#include <cstdint>
#include <unordered_map>
#include <mutex>
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

namespace {
// See StrictCommaScope (api.hpp). Thread-local so a strict parse on one
// thread cannot change what another thread is parsing.
thread_local bool g_strictCommas = false;
} // namespace

StrictCommaScope::StrictCommaScope() : previous_(g_strictCommas) { g_strictCommas = true; }
StrictCommaScope::~StrictCommaScope() { g_strictCommas = previous_; }

bool strictCommasEnabled() { return g_strictCommas; }

std::vector<std::unique_ptr<ASTNode>> parseAst(const std::string& code, const std::string& origin, SourceMap* sourceMap) {
    // Stamps every node this parse builds with one treeId and a dense
    // slot, so a ScopeTable can address it without the node carrying a
    // Scope pointer of its own -- see ASTNode::slot().
    ParseNumberingScope numbering;

    ParserDriver driver(origin);
    driver.strictCommas = g_strictCommas;
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
    // Spans attachComments too: it builds CommentedExpr wrappers, and they
    // belong to the same tree as what they wrap.
    ParseNumberingScope numbering;
    auto ast = parseAst(code, origin); // propagates ParseError with the full diagnostic
    if (includeComments) {
        ast = attachComments(std::move(ast), code, origin);
    }
    return ast;
}

namespace {

// A "not found" that says where we actually looked. The old message named
// only the includer, so "searched relative to X" read as "only X was
// searched" -- which is how #503 came to be filed against a search that
// does cover the libraries folder.
std::string notFound(const char* noun, const std::string& filename, const std::string& currentFile) {
    std::string msg = std::string(noun) + " '" + filename + "' not found. Searched:";
    for (const auto& d : librarySearchDirs(currentFile)) {
        msg += "\n  " + d;
    }
    return msg;
}

} // namespace

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
    ParseNumberingScope numbering;   // spans attachComments -- see getASTFromString
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
                throw std::runtime_error(notFound("Included file", filename, currentFile));
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

namespace {

// The user's own libraries folder -- OpenSCAD's
// PlatformUtils::userLibraryPath(), and the one an installer or a git clone
// of BOSL2 lands in.
std::string userLibraryDir() {
#if defined(_WIN32)
    // ASK Windows where My Documents is rather than assuming
    // %USERPROFILE%\Documents. OneDrive's Known Folder Move -- on by default
    // on a new machine -- relocates it to %USERPROFILE%\OneDrive\Documents,
    // and a library OpenSCAD itself installed then sat somewhere we never
    // looked (BelfrySCAD #503). Same call and same flag OpenSCAD makes
    // (PlatformUtils-win.cc getFolderPath).
    wchar_t buf[MAX_PATH] = {0};
    if (SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, buf) == S_OK) {
        return (fs::path(buf) / "OpenSCAD" / "libraries").string();
    }
    const char* userProfile = std::getenv("USERPROFILE");
    return userProfile ? (fs::path(userProfile) / "Documents" / "OpenSCAD" / "libraries").string()
                       : std::string();
#else
    const char* home = std::getenv("HOME");
    if (!home) {
        return {};
    }
#if defined(__APPLE__)
    return (fs::path(home) / "Documents" / "OpenSCAD" / "libraries").string();
#else
    return (fs::path(home) / ".local" / "share" / "OpenSCAD" / "libraries").string();
#endif
#endif
}

// Libraries shipped alongside this build -- OpenSCAD's
// PlatformUtils::resourcePath("libraries"). "Alongside" means beside the
// binary this code was linked into: the CLI executable, or the Python
// extension inside its installed package.
// ponytail: one candidate directory, no ../share/openscad/libraries walk --
// add the walk if a packaging layout ever puts the libraries somewhere
// other than next to the binary.
std::string bundledLibraryDir() {
    fs::path self;
#if defined(_WIN32)
    HMODULE mod = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                               | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&userLibraryDir), &mod)) {
        wchar_t buf[MAX_PATH] = {0};
        if (GetModuleFileNameW(mod, buf, MAX_PATH)) {
            self = buf;
        }
    }
#else
    Dl_info info;
    if (dladdr(reinterpret_cast<void*>(&userLibraryDir), &info) && info.dli_fname) {
        self = info.dli_fname;
    }
#endif
    if (self.empty()) {
        return {};
    }
    std::error_code ec;
    return (fs::absolute(self, ec).parent_path() / "libraries").string();
}

} // namespace

std::vector<std::string> librarySearchDirs(const std::string& currFile) {
    std::vector<std::string> dirs;
    if (!currFile.empty()) {
        dirs.push_back(fs::absolute(currFile).parent_path().string());
    }

#if defined(_WIN32)
    const char pathsep = ';';
#else
    const char pathsep = ':';
#endif

    // OPENSCADPATH comes FIRST and adds to the built-in paths rather than
    // replacing them -- exactly what OpenSCAD's parser_init() does. It used
    // to replace them, so setting the variable for one library hid every
    // other, including a BOSL2 sitting in the default folder (#503).
    const char* envPath = std::getenv("OPENSCADPATH");
    if (envPath) {
        std::string env(envPath);
        size_t start = 0;
        while (start <= env.size()) {
            size_t pos = env.find(pathsep, start);
            std::string part = (pos == std::string::npos) ? env.substr(start) : env.substr(start, pos - start);
            if (!part.empty()) {
                dirs.push_back(part);
            }
            if (pos == std::string::npos) {
                break;
            }
            start = pos + 1;
        }
    }

    for (const std::string& d : {userLibraryDir(), bundledLibraryDir()}) {
        if (!d.empty()) {
            dirs.push_back(d);
        }
    }
    return dirs;
}

std::optional<std::string> findLibraryFile(const std::string& currFile, const std::string& libFile) {
    for (const auto& d : librarySearchDirs(currFile)) {
        fs::path candidate = fs::path(d) / libFile;
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
        throw std::runtime_error(notFound("Library file", libFile, currFile));
    }
    auto ast = getASTFromFile(*found, includeComments, processIncludes);
    return LibraryFileResult{std::move(ast), *found};
}



namespace {

using FileAst = std::vector<std::unique_ptr<ASTNode>>;
using FileAstPtr = std::shared_ptr<const FileAst>;

// One entry per (file, comments, strict-commas) -- keyed by PATH, with the
// content stamp
// stored beside the tree rather than in the key. A stale stamp REPLACES the
// entry instead of adding a second one: an editor re-renders on every save,
// and a stamp-in-the-key cache would keep a full copy of every version the
// file ever had.
struct CacheKey {
    std::string path;
    bool comments;
    // Part of the key, not incidental: the same file parses differently
    // under StrictCommaScope, so a strict parse must not be served a tree an
    // earlier lenient parse of it left here (and vice versa).
    bool strictCommas;
    bool operator==(const CacheKey& o) const {
        return comments == o.comments && strictCommas == o.strictCommas && path == o.path;
    }
};
struct CacheKeyHash {
    size_t operator()(const CacheKey& k) const {
        return std::hash<std::string>{}(k.path) ^ (k.comments ? 0x5bf03635U : 0U)
               ^ (k.strictCommas ? 0x9e3779b9U : 0U);
    }
};
struct CacheEntry {
    std::uintmax_t size = 0;
    std::int64_t mtime = 0;
    FileAstPtr ast;
};

std::mutex g_astCacheMutex;
std::unordered_map<CacheKey, CacheEntry, CacheKeyHash> g_astCache;

FileAstPtr parseFileShared(const std::string& absPath, bool includeComments) {
    std::error_code ec;
    const auto size = fs::file_size(absPath, ec);
    const std::uintmax_t stampSize = ec ? 0 : size;
    ec.clear();
    const auto written = fs::last_write_time(absPath, ec);
    const std::int64_t stampMtime = ec ? 0 : static_cast<std::int64_t>(written.time_since_epoch().count());

    const CacheKey key{absPath, includeComments, g_strictCommas};
    {
        std::lock_guard<std::mutex> lock(g_astCacheMutex);
        auto it = g_astCache.find(key);
        if (it != g_astCache.end() && it->second.size == stampSize && it->second.mtime == stampMtime)
            return it->second.ast;
    }

    // Parsed OUTSIDE the lock: parsing a library takes tens of
    // milliseconds, and holding a global lock across it would serialise
    // every thread. Two threads racing the same file both parse and one
    // result is dropped -- wasteful once, never wrong, and far cheaper than
    // the alternative.
    auto parsed = std::make_shared<const FileAst>(parseSingleFile(absPath, includeComments));
    std::lock_guard<std::mutex> lock(g_astCacheMutex);
    CacheEntry& entry = g_astCache[key];
    // Whoever writes last wins; the loser's tree stays alive in whatever
    // ParsedProgram already borrowed it.
    entry.size = stampSize;
    entry.mtime = stampMtime;
    entry.ast = parsed;
    return parsed;
}

// Splices includes into a flat statement list of BORROWED nodes, collecting
// what must stay alive. Mirrors resolveIncludes' walk exactly, including
// the per-resolution `visited` set that makes a file contribute at most
// once.
void collectProgram(const FileAst& nodes, const std::string& currentFile, bool includeComments,
                    std::set<std::string>& visited, ParsedProgram& out) {
    for (const auto& node : nodes) {
        if (node->kind() == NodeKind::IncludeStatement) {
            const auto& inc = static_cast<const IncludeStatement&>(*node);
            const std::string& filename = inc.filepath->val;
            auto libFile = findLibraryFile(currentFile, filename);
            if (!libFile) {
                throw std::runtime_error(notFound("Included file", filename, currentFile));
            }
            std::string absLib = fs::absolute(*libFile).string();
            if (!visited.insert(absLib).second) continue;
            FileAstPtr included = parseFileShared(absLib, includeComments);
            out.keepAlive.push_back(included);
            collectProgram(*included, absLib, includeComments, visited, out);
        } else {
            out.nodes.push_back(node.get());
        }
    }
}

} // namespace

ParsedProgram getProgramFromFile(const std::string& file, bool includeComments) {
    const std::string abs = fs::absolute(file).string();
    ParsedProgram out;
    FileAstPtr own = parseFileShared(abs, includeComments);
    out.keepAlive.push_back(own);
    std::set<std::string> visited{abs};
    collectProgram(*own, abs, includeComments, visited, out);
    return out;
}

void clearAstCache() {
    std::lock_guard<std::mutex> lock(g_astCacheMutex);
    g_astCache.clear();
}

size_t astCacheSize() {
    std::lock_guard<std::mutex> lock(g_astCacheMutex);
    return g_astCache.size();
}

} // namespace oscad
