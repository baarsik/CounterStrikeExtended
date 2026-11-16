#pragma once

#include <cmath>
#include <locale>
#include <sstream>
#include <string>
#include <string_view>

// libc++ floating-point from_chars is unavailable at the macOS 14 deployment
// target. Classic-locale streams preserve map parsing independently of locale.
inline bool NativeRadar_ParseFloat(std::string_view text, float& value, std::size_t& consumed) {
    consumed = 0;
    if (text.empty() || text.front() == '+') {
        return false;
    }
    std::istringstream input{std::string(text)};
    input.imbue(std::locale::classic());
    input >> std::noskipws >> value;
    if (input.fail() || !std::isfinite(value)) {
        return false;
    }
    consumed = input.eof() ? text.size() : static_cast<std::size_t>(input.tellg());
    return consumed > 0;
}
