#pragma once

namespace oscad {

// Custom bison location type: flat first/last line+column+byte-offset
// fields (classic-yacc YYLTYPE style), rather than bison's default C++
// skeleton begin/end `position` sub-objects -- because we need byte
// offsets (for Position::start_offset/end_offset) and the default location
// class has no such field. See YYLLOC_DEFAULT override in parser.y for the
// merging logic this requires.
struct OscadLocation {
    int first_line = 1;
    int first_column = 1;
    int last_line = 1;
    int last_column = 1;
    int first_offset = 0;
    int last_offset = 0;
};

} // namespace oscad
