#pragma once

// Internal helper shared by the AST classes' toString() implementations.
// Not installed -- not part of the public API.

#include <memory>
#include <string>
#include <vector>

namespace oscad {

template <typename T>
std::string joinToString(const std::vector<std::unique_ptr<T>>& items, const char* sep) {
    std::string result;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i != 0) {
            result += sep;
        }
        result += items[i]->toString();
    }
    return result;
}

inline std::string join(const std::vector<std::string>& items, const std::string& sep) {
    std::string result;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i != 0) {
            result += sep;
        }
        result += items[i];
    }
    return result;
}

} // namespace oscad
