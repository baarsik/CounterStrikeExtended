#pragma once

#include "../../../common/legacy_text_encoding.h"

#include <cwchar>
#include <string>

namespace cse::forms {

inline std::wstring WideText(const std::string& text) {
    std::wstring result(text.size() + 1, L'\0');
    legacy_text::Utf8ToWideCells(text.c_str(), result.data(), static_cast<int>(result.size() * sizeof(wchar_t)));
    result.resize(std::wcslen(result.data()));
    return result;
}

inline std::wstring Caption(const std::string& text, char key = '\0') {
    std::wstring result;
    for (wchar_t character : WideText(text)) {
        // TextImage treats a single ampersand as a mnemonic marker.
        if (character == L'&') {
            result += L'&';
        }
        result += character;
    }
    if (key) {
        const wchar_t displayKey = key >= 'a' && key <= 'z' ? key - 'a' + L'A' : key;
        if (!result.empty()) {
            result += L' ';
        }
        result += L'[';
        result += displayKey;
        result += L']';
    }
    return result;
}

inline std::wstring FormWindowTitle(const std::string& title) {
    return L"Server form - " + Caption(title);
}

}
