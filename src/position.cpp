#include "openscad_cpp_parser/position.hpp"

namespace oscad {

std::string toString(const Position& pos) {
    return pos.origin + ":" + std::to_string(pos.line) + ":" + std::to_string(pos.column) + "[" +
           std::to_string(pos.start_offset) + ":" + std::to_string(pos.end_offset) + "]";
}

} // namespace oscad
