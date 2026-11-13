#include "VGUI_App.h"
#include "VGUI_SurfaceBase.h"
#include "../../../../../common/mac_settings_store.h"
#include "../../../../../common/sdl_ui_clipboard.h"
#include "../common/VGui1LegacyTickMillis.h"
#include "../posix/native/VGui1LegacyRaster.h"
#include <SDL3/SDL.h>

namespace {
std::string LegacyClipboard() {
    const auto utf8 = cse::ui_clipboard::ReadUtf8();
    std::string bytes;
    const auto* cursor = reinterpret_cast<const unsigned char*>(utf8.c_str());
    constexpr unsigned int FirstExtendedByte = 0x80;
    constexpr unsigned int FirstLatin1Byte = 0xa0;
    constexpr unsigned int MaximumByte = 0xff;
    while (*cursor) {
        const auto decoded = cse::legacy_text::detail::DecodeUtf8(cursor);
        unsigned char byte = '?';
        if (decoded.scalar < FirstExtendedByte
            || (decoded.scalar >= FirstLatin1Byte && decoded.scalar <= MaximumByte)) {
            byte = static_cast<unsigned char>(decoded.scalar);
        } else {
            for (unsigned int candidate = FirstExtendedByte; candidate < FirstLatin1Byte; ++candidate) {
                if (cse_vgui1::legacy::LegacyByteToUnicode(static_cast<unsigned char>(candidate)) == decoded.scalar) {
                    byte = static_cast<unsigned char>(candidate);
                    break;
                }
            }
        }
        bytes += static_cast<char>(byte);
        cursor += decoded.consumed;
    }
    return bytes;
}
}

namespace vgui {

void App::platTick() {
    // The engine owns event dispatch; pumping must not consume its queue.
    SDL_PumpEvents();
}

void App::setCursorPos(int, int) {
    // Retained App semantics leave cursor warping to the surface/input owner.
}

void App::getCursorPos(int& x, int& y) {
    x = 0;
    y = 0;
    if (_surfaceBaseDar.getCount() > 0 && _surfaceBaseDar[0]) {
        _surfaceBaseDar[0]->GetMousePos(x, y);
    }
}

void App::internalSetMouseArena(int x0, int y0, int x1, int y1, bool enabled) {
    SDL_Window* window = SDL_GetMouseFocus();
    if (!window) {
        return;
    }
    int x = 0;
    int y = 0;
    SDL_GetWindowPosition(window, &x, &y);
    const SDL_Rect bounds{x0 - x, y0 - y, x1 - x0, y1 - y0};
    SDL_SetWindowMouseRect(window, enabled ? &bounds : nullptr);
}

long App::getTimeMillis() {
    return detail::SignedTickMillis(static_cast<std::uint32_t>(SDL_GetTicks()));
}

void App::setClipboardText(const char* text, int length) {
    if (!text || length < 0) {
        return;
    }
    std::vector<wchar_t> cells(static_cast<std::size_t>(length));
    for (int index = 0; index < length; ++index) {
        cells[index] = cse_vgui1::legacy::LegacyByteToUnicode(static_cast<unsigned char>(text[index]));
    }
    cse::ui_clipboard::Write(cells.data(), length);
}

int App::getClipboardTextCount() {
    const auto bytes = LegacyClipboard();
    return bytes.size() < static_cast<std::size_t>((std::numeric_limits<int>::max)())
        ? static_cast<int>(bytes.size() + 1) : 0;
}

int App::getClipboardText(int offset, char* destination, int capacity) {
    if (offset < 0 || !destination || capacity <= 0) {
        return 0;
    }
    const auto bytes = LegacyClipboard();
    const auto start = static_cast<std::size_t>(offset);
    if (start > bytes.size()) {
        return 0;
    }
    const auto count = std::min(bytes.size() + 1 - start, static_cast<std::size_t>(capacity));
    std::memcpy(destination, bytes.c_str() + start, count);
    return static_cast<int>(count);
}

bool App::setRegistryString(const char* key, const char* value) {
    return cse::mac_settings::WriteString(key, value);
}

bool App::getRegistryString(const char* key, char* value, int capacity) {
    return capacity >= 0 && cse::mac_settings::ReadBytes(key, value, static_cast<std::size_t>(capacity));
}

bool App::setRegistryInteger(const char* key, int value) {
    return cse::mac_settings::WriteInt32(key, value);
}

bool App::getRegistryInteger(const char* key, int& value) {
    return cse::mac_settings::ReadInt32(key, value);
}

App* App::getInstance() {
    return _instance;
}

} // namespace vgui
