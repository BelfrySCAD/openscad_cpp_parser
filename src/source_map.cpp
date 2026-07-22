#include "openscad_cpp_parser/source_map.hpp"

#include "openscad_cpp_parser/api.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

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

int skipWhitespace(const std::string& code, int start) {
    int i = start;
    int n = static_cast<int>(code.size());
    while (i < n) {
        char c = code[static_cast<size_t>(i)];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            break;
        }
        ++i;
    }
    return i;
}

struct IncludeInfo {
    int position;
    int length;
    std::string filename;
};

// Mirrors source_map.py::_find_valid_includes: a hand-rolled scanner that
// finds `include <file>` directives while skipping over string and comment
// content (so `"see include <x> in docs"` or `// include <x>` don't match).
std::vector<IncludeInfo> findValidIncludes(const std::string& code) {
    std::vector<IncludeInfo> includes;
    int i = 0;
    int n = static_cast<int>(code.size());
    bool inString = false;
    char stringChar = '\0';
    bool inSingleLineComment = false;
    bool inMultiLineComment = false;

    while (i < n) {
        char ch = code[static_cast<size_t>(i)];
        bool hasNext = (i + 1 < n);
        char nextChar = hasNext ? code[static_cast<size_t>(i + 1)] : '\0';

        if (!inSingleLineComment && !inMultiLineComment) {
            if (ch == '"' || ch == '\'') {
                if (!inString) {
                    inString = true;
                    stringChar = ch;
                } else if (ch == stringChar) {
                    if (i > 0 && code[static_cast<size_t>(i - 1)] != '\\') {
                        inString = false;
                        stringChar = '\0';
                    }
                }
            } else if (inString && ch == '\\' && hasNext && nextChar == stringChar) {
                ++i;
            }
        }
        if (!inString && !inMultiLineComment) {
            if (ch == '/' && hasNext && nextChar == '/') {
                inSingleLineComment = true;
                ++i;
            } else if (inSingleLineComment && ch == '\n') {
                inSingleLineComment = false;
            }
        }
        if (!inString && !inSingleLineComment) {
            if (ch == '/' && hasNext && nextChar == '*') {
                inMultiLineComment = true;
                ++i;
            } else if (inMultiLineComment && ch == '*' && hasNext && nextChar == '/') {
                inMultiLineComment = false;
                ++i;
            }
        }
        if (!inString && !inSingleLineComment && !inMultiLineComment) {
            bool looksLikeInclude =
                ch == 'i' && i + 7 < n && code.compare(static_cast<size_t>(i), 7, "include") == 0 &&
                (i == 0 ||
                 !(std::isalnum(static_cast<unsigned char>(code[static_cast<size_t>(i - 1)])) != 0 ||
                   code[static_cast<size_t>(i - 1)] == '_')) &&
                i + 8 < n;
            if (looksLikeInclude) {
                int afterInclude = skipWhitespace(code, i + 7);
                if (afterInclude < n && code[static_cast<size_t>(afterInclude)] == '<') {
                    int startPos = i;
                    int filenameStart = afterInclude + 1;
                    int filenameEnd = filenameStart;
                    bool hitNewline = false;
                    while (filenameEnd < n && code[static_cast<size_t>(filenameEnd)] != '>') {
                        if (code[static_cast<size_t>(filenameEnd)] == '\n') {
                            hitNewline = true;
                            break;
                        }
                        ++filenameEnd;
                    }
                    if (!hitNewline && filenameEnd < n) {
                        std::string filename =
                            strip(code.substr(static_cast<size_t>(filenameStart), static_cast<size_t>(filenameEnd - filenameStart)));
                        if (!filename.empty()) {
                            int endPos = filenameEnd + 1;
                            includes.push_back(IncludeInfo{startPos, endPos - startPos, filename});
                            i = endPos;
                            continue;
                        }
                    }
                }
            }
        }
        ++i;
    }
    return includes;
}

} // namespace

int SourceMap::addOrigin(const std::string& origin, const std::string& content, std::optional<int> insertAtOpt,
                          int startLine, int startColumn, int replaceLength, bool stripTrailingNewline) {
    int insertAt;
    if (insertAtOpt.has_value()) {
        insertAt = *insertAtOpt;
    } else if (!segments_.empty()) {
        insertAt = 0;
        for (const auto& seg : segments_) {
            insertAt = std::max(insertAt, seg.combined_start + static_cast<int>(seg.content.size()));
        }
    } else {
        insertAt = 0;
    }

    if (replaceLength > 0) {
        replaceText(insertAt, replaceLength, stripTrailingNewline);
    }

    SourceSegment segment;
    segment.origin = origin;
    segment.start_line = startLine;
    segment.start_column = startColumn;
    segment.content = content;
    segment.combined_start = insertAt;
    int result = segment.combined_start;

    insertSegment(std::move(segment), insertAt);
    combinedStringDirty_ = true;
    return result;
}

void SourceMap::replaceText(int startPos, int length, bool stripTrailingNewline) {
    int endPos = startPos + length;
    std::vector<size_t> indicesToRemove;
    std::vector<SourceSegment> newSegments;

    for (size_t idx = 0; idx < segments_.size(); ++idx) {
        SourceSegment& segment = segments_[idx];
        int segmentStart = segment.combined_start;
        int segmentEnd = segmentStart + static_cast<int>(segment.content.size());
        if (segmentStart >= endPos || segmentEnd <= startPos) {
            continue;
        }

        int replaceStartInSegment = std::max(0, startPos - segmentStart);
        int replaceEndInSegment = std::min(static_cast<int>(segment.content.size()), endPos - segmentStart);
        // Snapshot before mutating segment.content below -- both
        // before/after content AND the removed-span newline count (used
        // for the after-segment's new start_line/start_column) must be
        // computed from the ORIGINAL content, not the shortened one.
        std::string originalContent = segment.content;
        std::string beforeContent = originalContent.substr(0, static_cast<size_t>(replaceStartInSegment));
        std::string afterContent = originalContent.substr(static_cast<size_t>(replaceEndInSegment));

        if (!beforeContent.empty()) {
            segment.content = beforeContent;
        } else {
            indicesToRemove.push_back(idx);
        }

        if (afterContent.empty()) {
            continue;
        }

        int lineCountAdjustment = 0;
        if (stripTrailingNewline && afterContent.front() == '\n') {
            afterContent = afterContent.substr(1);
            lineCountAdjustment = 1;
        }
        if (afterContent.empty()) {
            continue;
        }

        SourceSegment afterSegment;
        afterSegment.origin = segment.origin;
        afterSegment.start_line = segment.start_line;
        afterSegment.start_column = segment.start_column;
        afterSegment.content = afterContent;
        afterSegment.combined_start = startPos;

        std::string removedAndBefore = originalContent.substr(0, static_cast<size_t>(replaceEndInSegment));
        int newlineCount = static_cast<int>(std::count(removedAndBefore.begin(), removedAndBefore.end(), '\n'));
        int lineCount = newlineCount + lineCountAdjustment;
        if (lineCount > 0) {
            if (lineCountAdjustment != 0 && newlineCount == 0) {
                afterSegment.start_line = segment.start_line + lineCount;
                afterSegment.start_column = 1;
            } else {
                size_t lastNewline = removedAndBefore.rfind('\n');
                afterSegment.start_line = segment.start_line + lineCount;
                afterSegment.start_column = static_cast<int>(removedAndBefore.size() - lastNewline);
            }
        } else {
            afterSegment.start_line = segment.start_line;
            afterSegment.start_column = segment.start_column + static_cast<int>(removedAndBefore.size());
        }
        newSegments.push_back(std::move(afterSegment));
    }

    std::sort(indicesToRemove.rbegin(), indicesToRemove.rend());
    for (size_t idx : indicesToRemove) {
        segments_.erase(segments_.begin() + static_cast<long>(idx));
    }
    for (auto& seg : newSegments) {
        segments_.push_back(std::move(seg));
    }
    segments_.erase(std::remove_if(segments_.begin(), segments_.end(),
                                    [](const SourceSegment& s) { return s.content.empty(); }),
                     segments_.end());

    for (auto& segment : segments_) {
        if (segment.combined_start >= endPos) {
            segment.combined_start -= length;
        }
    }
}

void SourceMap::insertSegment(SourceSegment segment, int insertAt) {
    int segmentLength = static_cast<int>(segment.content.size());
    for (auto& existing : segments_) {
        if (existing.combined_start >= insertAt) {
            existing.combined_start += segmentLength;
        }
    }
    size_t insertIdx = segments_.size();
    for (size_t i = 0; i < segments_.size(); ++i) {
        if (segments_[i].combined_start > segment.combined_start) {
            insertIdx = i;
            break;
        }
    }
    segments_.insert(segments_.begin() + static_cast<long>(insertIdx), std::move(segment));
}

const std::string& SourceMap::getCombinedString() {
    if (combinedStringDirty_) {
        rebuildCombinedString();
    }
    return combinedString_;
}

void SourceMap::rebuildCombinedString() {
    if (segments_.empty()) {
        combinedString_.clear();
        combinedStringDirty_ = false;
        return;
    }
    std::vector<const SourceSegment*> sorted;
    sorted.reserve(segments_.size());
    for (const auto& s : segments_) {
        sorted.push_back(&s);
    }
    std::sort(sorted.begin(), sorted.end(),
              [](const SourceSegment* a, const SourceSegment* b) { return a->combined_start < b->combined_start; });

    std::string result;
    int currentPos = 0;
    for (const auto* seg : sorted) {
        if (seg->combined_start > currentPos) {
            result.append(static_cast<size_t>(seg->combined_start - currentPos), ' ');
        }
        result += seg->content;
        currentPos = seg->combined_start + static_cast<int>(seg->content.size());
    }
    combinedString_ = std::move(result);
    combinedStringDirty_ = false;
}

// ponytail: linear scan rather than Python's binary search. _replace_text
// can append "after" remainder segments out of combined_start order (via
// vector::push_back rather than a sorted insert), so a binary search would
// depend on a sort invariant that isn't actually maintained end-to-end. A
// linear scan is correct regardless of ordering and segment counts here
// are small (a handful of included files), so the O(n) cost is a non-issue.
const SourceSegment* SourceMap::findSegment(int position) const {
    for (const auto& seg : segments_) {
        int segEnd = seg.combined_start + static_cast<int>(seg.content.size());
        if (seg.combined_start <= position && position < segEnd) {
            return &seg;
        }
    }
    return nullptr;
}

Position SourceMap::calculateLocationInSegment(const SourceSegment& segment, int offset,
                                                std::optional<int> endOffset) const {
    if (offset < 0) {
        offset = 0;
    }
    if (offset > static_cast<int>(segment.content.size())) {
        offset = static_cast<int>(segment.content.size());
    }
    std::string contentBefore = segment.content.substr(0, static_cast<size_t>(offset));
    int lineCount = static_cast<int>(std::count(contentBefore.begin(), contentBefore.end(), '\n'));
    int lineNumber = segment.start_line + lineCount;
    int columnNumber;
    if (lineCount == 0) {
        columnNumber = segment.start_column + offset;
    } else {
        size_t lastNewline = contentBefore.rfind('\n');
        columnNumber = offset - static_cast<int>(lastNewline);
    }
    int resolvedEnd = endOffset.has_value() ? *endOffset : offset;
    return Position{segment.origin, lineNumber, columnNumber, offset, resolvedEnd};
}

Position SourceMap::getLocation(int position, std::optional<int> endPosition) {
    if (position < 0) {
        position = 0;
    }
    const SourceSegment* segment = findSegment(position);
    if (segment == nullptr) {
        if (!segments_.empty()) {
            const SourceSegment* last = &segments_[0];
            for (const auto& s : segments_) {
                int sEnd = s.combined_start + static_cast<int>(s.content.size());
                int lastEnd = last->combined_start + static_cast<int>(last->content.size());
                if (sEnd > lastEnd) {
                    last = &s;
                }
            }
            int segOffset = static_cast<int>(last->content.size());
            int endOffset = endPosition.has_value() ? (*endPosition - last->combined_start) : segOffset;
            return calculateLocationInSegment(*last, segOffset, endOffset);
        }
        return Position{"", 1, 1, 0, 0};
    }
    int segmentOffset = position - segment->combined_start;
    int endOffset = endPosition.has_value() ? (*endPosition - segment->combined_start) : segmentOffset;
    return calculateLocationInSegment(*segment, segmentOffset, endOffset);
}

SourceMap createSourceMapFromOrigins(const std::vector<std::pair<std::string, std::string>>& origins,
                                      std::optional<std::vector<int>> insertPositions) {
    SourceMap sourceMap;
    if (!insertPositions.has_value()) {
        for (const auto& originContent : origins) {
            sourceMap.addOrigin(originContent.first, originContent.second);
        }
    } else {
        if (insertPositions->size() != origins.size()) {
            throw std::invalid_argument("insertPositions must have same length as origins");
        }
        for (size_t i = 0; i < origins.size(); ++i) {
            sourceMap.addOrigin(origins[i].first, origins[i].second, (*insertPositions)[i]);
        }
    }
    return sourceMap;
}

void processIncludes(SourceMap& sourceMap, const std::string& currentFile, int maxIterations) {
    std::string curFile = currentFile;
    int iteration = 0;
    bool exhausted = true;

    while (iteration < maxIterations) {
        ++iteration;
        std::string combined = sourceMap.getCombinedString();
        auto includes = findValidIncludes(combined);
        if (includes.empty()) {
            exhausted = false;
            break;
        }
        std::sort(includes.begin(), includes.end(),
                  [](const IncludeInfo& a, const IncludeInfo& b) { return a.position > b.position; });

        for (const auto& info : includes) {
            auto libFile = findLibraryFile(curFile, info.filename);
            if (!libFile) {
                throw std::runtime_error("Included file '" + info.filename + "' not found. Searched relative to: " +
                                          (curFile.empty() ? "current directory" : curFile));
            }
            std::ifstream in(*libFile, std::ios::binary);
            if (!in) {
                throw std::runtime_error("Error reading included file '" + *libFile + "'");
            }
            std::ostringstream ss;
            ss << in.rdbuf();
            sourceMap.addOrigin(*libFile, ss.str(), info.position, 1, 1, info.length, /*stripTrailingNewline=*/true);
            curFile = *libFile;
        }
    }

    if (exhausted) {
        throw std::runtime_error("Maximum iterations (" + std::to_string(maxIterations) +
                                  ") exceeded while processing includes. This may indicate circular includes or a very "
                                  "deep include chain.");
    }
}

} // namespace oscad
