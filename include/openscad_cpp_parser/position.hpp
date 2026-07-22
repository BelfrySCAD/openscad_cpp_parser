#pragma once

#include <string>

namespace oscad {

// One Position per node encodes a full source span via start/end byte
// offsets (0-based), plus the 1-indexed line/column of the span's start.
struct Position {
    std::string origin;
    int line = 1;
    int column = 1;
    int start_offset = 0;
    int end_offset = 0;
};

// "{origin}:{line}:{column}[{start_offset}:{end_offset}]"
std::string toString(const Position& pos);

} // namespace oscad
