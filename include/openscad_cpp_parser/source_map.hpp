#pragma once

#include "openscad_cpp_parser/position.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace oscad {

// A contiguous run of one origin's content within a SourceMap's combined
// buffer. Mirrors source_map.py::SourceSegment.
struct SourceSegment {
    std::string origin;
    int start_line = 1;
    int start_column = 1;
    std::string content;
    int combined_start = 0;
};

// Combines multiple source origins (files, editor buffers, ...) into a
// single string for parsing, while retaining the ability to map a byte
// offset in that combined string back to its original (origin, line,
// column). Used to give accurate diagnostics/positions when `include
// <...>` directives are textually spliced into one buffer before a single
// parse (see processIncludes below). Mirrors source_map.py::SourceMap.
class SourceMap {
public:
    // Adds `content` (attributed to `origin`) into the combined buffer at
    // `insertAt` (default: append at the end). If replaceLength > 0, first
    // removes that many combined-buffer characters starting at insertAt
    // (used by processIncludes to splice out an `include <...>` directive
    // and splice in the target file's content in its place). Returns the
    // combined-buffer offset the segment was inserted at.
    int addOrigin(const std::string& origin, const std::string& content, std::optional<int> insertAt = std::nullopt,
                  int startLine = 1, int startColumn = 1, int replaceLength = 0, bool stripTrailingNewline = false);

    const std::string& getCombinedString();

    // Maps a combined-buffer byte offset (and optional end offset, for a
    // span) back to a Position in its original origin.
    Position getLocation(int position, std::optional<int> endPosition = std::nullopt);

    const std::vector<SourceSegment>& getSegments() const { return segments_; }

private:
    void replaceText(int startPos, int length, bool stripTrailingNewline);
    void insertSegment(SourceSegment segment, int insertAt);
    void rebuildCombinedString();
    const SourceSegment* findSegment(int position) const;
    Position calculateLocationInSegment(const SourceSegment& segment, int offset, std::optional<int> endOffset) const;

    std::vector<SourceSegment> segments_;
    std::string combinedString_;
    bool combinedStringDirty_ = true;
};

// Builds a SourceMap from a list of (origin, content) pairs, either
// appended in order or placed at explicit combined-buffer offsets.
SourceMap createSourceMapFromOrigins(const std::vector<std::pair<std::string, std::string>>& origins,
                                      std::optional<std::vector<int>> insertPositions = std::nullopt);

// Repeatedly scans the combined buffer for `include <file>` directives
// (skipping directives inside string/comment text), resolves each via
// findLibraryFile(), and splices the target file's content into the
// SourceMap in place of the directive -- so a single subsequent parse sees
// one fully-expanded buffer. Cycle/depth-guarded via maxIterations (throws
// std::runtime_error if exceeded, mirroring the Python ValueError).
void processIncludes(SourceMap& sourceMap, const std::string& currentFile = "", int maxIterations = 100);

} // namespace oscad
