#include "openscad_cpp_parser/comments.hpp"

#include "comment_attach_internal.hpp"

#include <algorithm>

namespace oscad {

namespace {

std::string strip(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(start, end - start);
}

Position positionFromOffsets(const std::string& code, const std::string& origin, size_t start, size_t end) {
    int line = 1;
    for (size_t k = 0; k < start && k < code.size(); ++k) {
        if (code[k] == '\n') {
            ++line;
        }
    }
    size_t lastNl = std::string::npos;
    if (start > 0) {
        lastNl = code.rfind('\n', start - 1);
    }
    int col = (lastNl != std::string::npos) ? static_cast<int>(start - lastNl) : static_cast<int>(start + 1);
    return Position{origin, line, col, static_cast<int>(start), static_cast<int>(end)};
}

// Mirrors _extract_comments: scans raw source for `//...`/`/*...*/`,
// skipping over string literal contents (so a `"//not a comment"` inside a
// string isn't misdetected).
std::vector<std::unique_ptr<ASTNode>> extractComments(const std::string& code, const std::string& origin) {
    std::vector<std::unique_ptr<ASTNode>> comments;
    size_t i = 0;
    size_t n = code.size();
    while (i < n) {
        if (code[i] == '"') {
            size_t j = i + 1;
            while (j < n && code[j] != '"') {
                j += (code[j] == '\\' && j + 1 < n) ? 2 : 1;
            }
            i = (j < n) ? j + 1 : j;
            continue;
        }
        if (code[i] == '/' && i + 1 < n && code[i + 1] == '/') {
            size_t start = i;
            size_t j = i + 2;
            while (j < n && code[j] != '\n') {
                ++j;
            }
            std::string text = code.substr(i + 2, j - (i + 2));
            comments.push_back(std::make_unique<CommentLine>(positionFromOffsets(code, origin, start, j), std::move(text)));
            i = j;
            continue;
        }
        if (code[i] == '/' && i + 1 < n && code[i + 1] == '*') {
            size_t start = i;
            size_t j = i + 2;
            while (j + 1 < n && !(code[j] == '*' && code[j + 1] == '/')) {
                ++j;
            }
            bool found = (j + 1 < n);
            size_t textEnd = found ? j : n;
            size_t end = found ? j + 2 : n;
            std::string text = code.substr(i + 2, textEnd - (i + 2));
            comments.push_back(std::make_unique<CommentSpan>(positionFromOffsets(code, origin, start, end), std::move(text)));
            i = end;
            continue;
        }
        ++i;
    }
    return comments;
}

bool isInlineComment(const ASTNode& comment, const std::string& code) {
    size_t start = static_cast<size_t>(comment.position().start_offset);
    size_t end = static_cast<size_t>(comment.position().end_offset);
    size_t lineStart = 0;
    if (start > 0) {
        size_t p = code.rfind('\n', start - 1);
        lineStart = (p == std::string::npos) ? 0 : p + 1;
    }
    std::string before = strip(code.substr(lineStart, start - lineStart));
    if (!before.empty()) {
        return true;
    }
    if (comment.kind() == NodeKind::CommentSpan) {
        size_t nl = code.find('\n', end);
        size_t lastLineEnd = (nl == std::string::npos) ? code.size() : nl;
        std::string after = strip(code.substr(end, lastLineEnd - end));
        if (!after.empty() && after.rfind("//", 0) != 0 && after.rfind("/*", 0) != 0) {
            return true;
        }
    }
    return false;
}

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

// Mirrors _inject_comments: merges standalone comments into the top-level
// node list in source order, re-inserting preserved blank lines for gaps
// between consecutive standalone comments.
std::vector<std::unique_ptr<ASTNode>> injectComments(std::vector<std::unique_ptr<ASTNode>> astNodes,
                                                       std::vector<std::unique_ptr<ASTNode>> comments,
                                                       const std::string& code, const std::string& origin) {
    if (comments.empty()) {
        return astNodes;
    }
    std::vector<std::string> lines = splitLines(code);
    std::vector<std::unique_ptr<ASTNode>> result;
    size_t commentIdx = 0;
    int prevEndLine = 0;

    for (auto& node : astNodes) {
        int nodeLine = node->position().line;
        while (commentIdx < comments.size() && comments[commentIdx]->position().line < nodeLine) {
            auto& comment = comments[commentIdx];
            int cl = comment->position().line;
            if (prevEndLine > 0 && cl - prevEndLine > 1) {
                for (int gapLine = prevEndLine + 1; gapLine < cl; ++gapLine) {
                    std::string lineContent =
                        (gapLine >= 1 && static_cast<size_t>(gapLine) <= lines.size()) ? lines[gapLine - 1] : "";
                    if (strip(lineContent).empty()) {
                        result.push_back(std::make_unique<BlankLine>(Position{origin, gapLine, 1, 0, 0}));
                    }
                }
            }
            if (comment->kind() == NodeKind::CommentLine) {
                prevEndLine = comment->position().line;
            } else {
                int newlineCount = 0;
                for (char ch : static_cast<const CommentSpan&>(*comment).text) {
                    if (ch == '\n') {
                        ++newlineCount;
                    }
                }
                prevEndLine = comment->position().line + newlineCount;
            }
            result.push_back(std::move(comment));
            ++commentIdx;
        }
        int nodeEndLine = node->position().line;
        if (node->position().end_offset > 0) {
            int count = 1;
            int limit = std::min<int>(node->position().end_offset, static_cast<int>(code.size()));
            for (int k = 0; k < limit; ++k) {
                if (code[static_cast<size_t>(k)] == '\n') {
                    ++count;
                }
            }
            nodeEndLine = count;
        }
        prevEndLine = std::max(prevEndLine, nodeEndLine);
        result.push_back(std::move(node));
    }
    while (commentIdx < comments.size()) {
        result.push_back(std::move(comments[commentIdx]));
        ++commentIdx;
    }
    return result;
}

} // namespace

std::vector<std::unique_ptr<ASTNode>> attachComments(std::vector<std::unique_ptr<ASTNode>> astNodes, const std::string& code,
                                                       const std::string& origin) {
    auto comments = extractComments(code, origin);
    std::vector<std::unique_ptr<ASTNode>> inlineComments;
    std::vector<std::unique_ptr<ASTNode>> standalone;
    for (auto& c : comments) {
        if (isInlineComment(*c, code)) {
            inlineComments.push_back(std::move(c));
        } else {
            standalone.push_back(std::move(c));
        }
    }
    attachInlineComments(astNodes, std::move(inlineComments));
    return injectComments(std::move(astNodes), std::move(standalone), code, origin);
}

} // namespace oscad
