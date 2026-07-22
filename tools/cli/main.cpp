#include "openscad_cpp_parser/api.hpp"
#include "openscad_cpp_parser/pretty_print.hpp"
#include "openscad_cpp_parser/serialization.hpp"

#include <csignal>
#include <iostream>
#include <sstream>
#include <string>

using namespace oscad;

namespace {

void printUsage() {
    std::cerr << "usage: openscad-cpp-parser [-j|-y|-r] [-c|-C] [-i] [--indent N] [FILE]\n"
                 "\n"
                 "Parse an OpenSCAD file and dump its AST as JSON (default), YAML, or\n"
                 "reformatted OpenSCAD source. Omit FILE or pass '-' to read from stdin.\n"
                 "\n"
                 "  -j, --json          Output AST as JSON (default).\n"
                 "  -y, --yaml          Output AST as YAML.\n"
                 "  -r, --format        Output reformatted OpenSCAD source (comments preserved).\n"
                 "  -c, --no-comments   Exclude comments from the output.\n"
                 "  -C, --with-comments Include comments in the output.\n"
                 "  -i, --no-includes   Do not expand include <...> statements.\n"
                 "      --indent N      Indentation width in spaces (default: 4).\n";
}

std::string readStdin() {
    std::ostringstream ss;
    ss << std::cin.rdbuf();
    return ss.str();
}

// Tracks the first flag seen in a mutually-exclusive group (e.g. -j/-y/-r)
// and reports an argparse-style error if a second, different flag from the
// same group shows up. Mirrors argparse's add_mutually_exclusive_group().
class ExclusiveGroup {
public:
    // Returns false (and has already printed an error) if this call
    // conflicts with an earlier flag in the group.
    bool see(const std::string& flag) {
        if (!first_.empty() && first_ != flag) {
            std::cerr << "openscad-cpp-parser: error: argument " << flag << ": not allowed with argument " << first_
                      << "\n";
            return false;
        }
        first_ = flag;
        return true;
    }

private:
    std::string first_;
};

} // namespace

int main(int argc, char** argv) {
#ifndef _WIN32
    // Ignore SIGPIPE so writing to a closed pipe (e.g. `... | head`) fails
    // the write (ostream failbit) instead of killing the process outright
    // -- lets us exit(0) gracefully below, mirroring the reference CLI's
    // `except BrokenPipeError: sys.exit(0)`.
    std::signal(SIGPIPE, SIG_IGN);
#endif

    enum class OutputMode { Json, Yaml, Format };
    OutputMode mode = OutputMode::Json;
    bool noComments = false;
    bool withComments = false;
    bool noIncludes = false;
    int indent = 4;
    std::string file;
    bool haveFile = false;

    ExclusiveGroup outputGroup;
    ExclusiveGroup commentsGroup;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-j" || arg == "--json") {
            if (!outputGroup.see("-j/--json")) return 2;
            mode = OutputMode::Json;
        } else if (arg == "-y" || arg == "--yaml") {
            if (!outputGroup.see("-y/--yaml")) return 2;
            mode = OutputMode::Yaml;
        } else if (arg == "-r" || arg == "--format") {
            if (!outputGroup.see("-r/--format")) return 2;
            mode = OutputMode::Format;
        } else if (arg == "-c" || arg == "--no-comments") {
            if (!commentsGroup.see("-c/--no-comments")) return 2;
            noComments = true;
        } else if (arg == "-C" || arg == "--with-comments") {
            if (!commentsGroup.see("-C/--with-comments")) return 2;
            withComments = true;
        } else if (arg == "-i" || arg == "--no-includes") {
            noIncludes = true;
        } else if (arg == "--indent") {
            if (i + 1 >= argc) {
                std::cerr << "openscad-cpp-parser: --indent requires a value\n";
                return 2;
            }
            indent = std::stoi(argv[++i]);
        } else if (arg == "-h" || arg == "--help") {
            printUsage();
            return 0;
        } else if (!haveFile) {
            file = arg;
            haveFile = true;
        } else {
            std::cerr << "openscad-cpp-parser: unexpected argument '" << arg << "'\n";
            return 2;
        }
    }

    bool includeComments = false;
    if (mode == OutputMode::Format) {
        includeComments = true;
    }
    if (noComments) {
        includeComments = false;
    }
    if (withComments) {
        includeComments = true;
    }

    std::vector<std::unique_ptr<ASTNode>> ast;
    try {
        if (!haveFile || file == "-") {
            std::string code = readStdin();
            ast = getASTFromString(code, includeComments);
        } else {
            ast = getASTFromFile(file, includeComments, !noIncludes);
        }
    } catch (const std::exception& e) {
        std::cerr << "openscad-cpp-parser: " << e.what() << "\n";
        return 1;
    }

    switch (mode) {
        case OutputMode::Format:
            std::cout << toOpenscad(ast, indent) << "\n";
            break;
        case OutputMode::Yaml:
            std::cout << astToYamlString(ast);
            break;
        case OutputMode::Json:
            std::cout << astToJsonString(ast, /*includePosition=*/true, indent) << "\n";
            break;
    }
    std::cout.flush();
    if (std::cout.fail()) {
        return 0; // broken pipe or similar -- exit quietly, not noisily
    }
    return 0;
}
