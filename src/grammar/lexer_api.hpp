#pragma once

#include <string>

namespace oscad {

// Feeds `source` into the (non-reentrant, global-state) flex scanner as the
// buffer to lex from. Must be paired with lexerEnd() once parsing finishes.
void lexerBeginString(const std::string& source);
void lexerEnd();

} // namespace oscad
