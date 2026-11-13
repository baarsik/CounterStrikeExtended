#pragma once

#include "legacy_text_encoding.h"
#include <SDL3/SDL_clipboard.h>
#include <SDL3/SDL_stdinc.h>
#include <algorithm>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace cse::ui_clipboard {

// SDL owns platform publication and any deferred host callbacks. Only copied
// UTF8 strings cross that boundary; no pointer into a game module is retained.
inline std::string ReadUtf8() {
    char* data = SDL_GetClipboardText();
    std::string text = data ? data : "";
    SDL_free(data);
    return text;
}

inline void Write(const char* text, int length) {
    if (text && length >= 0) {
        const std::string copy(text, static_cast<std::size_t>(length));
        SDL_SetClipboardText(copy.c_str());
    }
}

inline void Write(const wchar_t* text, int length) {
    constexpr int MaximumCells = (std::numeric_limits<int>::max)() / 4 - 1;
    if (!text || length < 0 || length > MaximumCells) {
        return;
    }
    const std::wstring copy(text, static_cast<std::size_t>(length));
    std::vector<char> utf8((copy.size() + 1) * 4);
    if (legacy_text::WideCellsToUtf8(copy.c_str(), utf8.data(), static_cast<int>(utf8.size())) > 0) {
        SDL_SetClipboardText(utf8.data());
    }
}

inline int Count() {
    const auto text = ReadUtf8();
    return text.size() < static_cast<std::size_t>((std::numeric_limits<int>::max)())
        ? static_cast<int>(text.size() + 1) : 0;
}

inline int ReadBytes(int offset, char* destination, int capacity) {
    if (offset < 0 || !destination || capacity <= 0) {
        return 0;
    }
    const auto text = ReadUtf8();
    const auto start = static_cast<std::size_t>(offset);
    if (start > text.size()) {
        return 0;
    }
    const auto bytes = std::min(text.size() + 1 - start, static_cast<std::size_t>(capacity));
    std::memcpy(destination, text.c_str() + start, bytes);
    return static_cast<int>(bytes);
}

inline std::vector<wchar_t> ReadCells() {
    const auto utf8 = ReadUtf8();
    constexpr std::size_t MaximumCells = (std::numeric_limits<int>::max)() / sizeof(wchar_t);
    if (utf8.size() >= MaximumCells) {
        return {};
    }
    std::vector<wchar_t> cells(utf8.size() + 1);
    const int count = legacy_text::Utf8ToWideCells(utf8.c_str(), cells.data(),
        static_cast<int>(cells.size() * sizeof(wchar_t)));
    cells.resize(count > 0 ? static_cast<std::size_t>(count) : 0);
    return cells;
}

} // namespace cse::ui_clipboard
