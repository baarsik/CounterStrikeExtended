#ifndef CSE_HL_LEGACY_MESSAGE_TEXT_H
#define CSE_HL_LEGACY_MESSAGE_TEXT_H

#include "../common/legacy_text_encoding.h"
#include "../engine/vgui/ISurface.h"
#include "../engine/vgui/IScheme.h"
#include "native_module.h"
#include "runtime_module_names.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

namespace cse {
namespace hud_message {

// These are borrowed views of the engine's font manager, never the standalone
// vgui2 surface. Keep them inside one Draw/scan call while its engine is alive.
struct CreditsMetrics {
    vgui2::ISurface* surface = nullptr;
    vgui2::HFont font = 0;
    int height = 0;

    explicit operator bool() const {
        return surface && font && height > 0;
    }

    int Width(int character) const {
        int a = 0;
        int b = 0;
        int c = 0;
        surface->GetCharABCwide(font, character, a, b, c);
        return a + b + c;
    }
};

inline CreditsMetrics BorrowCreditsMetrics(const void* drawCharacter) {
    if (!drawCharacter) {
        return {};
    }
    void* engineModule = nullptr;
    void* guiModule = cse_platform::FindLoadedModule(cse::runtime_modules::Vgui2);
#if defined(_WIN32)
    HMODULE owner = nullptr;
    constexpr DWORD BorrowOwnerFlags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
    if (!GetModuleHandleExA(BorrowOwnerFlags, reinterpret_cast<LPCSTR>(drawCharacter), &owner)) {
        return {};
    }
    engineModule = owner;
#else
    Dl_info owner{};
    if (!dladdr(drawCharacter, &owner) || !owner.dli_fname) {
        return {};
    }
    engineModule = cse_platform::FindLoadedModule(owner.dli_fname);
    if (!guiModule) {
        const std::string enginePath(owner.dli_fname);
        const auto separator = enginePath.find_last_of('/');
        if (separator != std::string::npos) {
            const std::string guiPath = enginePath.substr(0, separator + 1) + cse::runtime_modules::Vgui2;
            guiModule = cse_platform::FindLoadedModule(guiPath.c_str());
        }
    }
#endif
    const auto engineFactory = reinterpret_cast<CreateInterfaceFn>(
        cse_platform::GetSymbol(engineModule, cse_platform::InterfaceFactorySymbol));
    const auto guiFactory = reinterpret_cast<CreateInterfaceFn>(
        cse_platform::GetSymbol(guiModule, cse_platform::InterfaceFactorySymbol));
    if (!engineFactory || !guiFactory) {
        return {};
    }
    auto* surface = static_cast<vgui2::ISurface*>(engineFactory("VGUI_Surface026", nullptr));
    auto* manager = static_cast<vgui2::ISchemeManager*>(guiFactory("VGUI_Scheme009", nullptr));
    // The engine installs its root and Tracker scheme before exposing HUD draws.
    // No initialized root means the public scheme handle is not yet usable.
    if (!surface || !manager || !surface->GetEmbeddedPanel()) {
        return {};
    }
    const auto defaultScheme = manager->GetDefaultScheme();
    if (!defaultScheme) {
        return {};
    }
    auto* scheme = manager->GetIScheme(defaultScheme);
    if (!scheme) {
        return {};
    }
    const char* scaled = scheme->GetResourceString("UseScaledCreditsFont");
    const char* name = scaled && std::atoi(scaled) ? "CreditsFont" : "Legacy_CreditsFont";
    const auto font = scheme->GetFont(name, true);
    if (!font) {
        return {};
    }
    const int height = surface->GetFontTall(font);
    if (height <= 0) {
        return {};
    }
    return {surface, font, height};
}

// A complete message must be valid BMP UTF8 before changing its byte semantics.
// This preserves the original CP1252 resources and their byte draw callbacks.
inline bool IsBmpUtf8(const char* text) {
    if (!text) {
        return false;
    }
    const auto* input = reinterpret_cast<const unsigned char*>(text);
    while (*input) {
        const auto decoded = legacy_text::detail::DecodeUtf8(input);
        if (decoded.scalar >= legacy_text::detail::FirstSupplementary) {
            return false;
        }
        const auto encoded = legacy_text::detail::EncodeUtf8(decoded.scalar);
        if (decoded.consumed != encoded.count) {
            return false;
        }
        for (std::size_t index = 0; index < encoded.count; ++index) {
            if (input[index] != encoded.bytes[index]) {
                return false;
            }
        }
        input += decoded.consumed;
    }
    return true;
}

struct Text {
    std::vector<int> cells;
    bool unicode = false;

    Text(const char* source, const CreditsMetrics& metrics) {
        if (!source) {
            return;
        }
        unicode = static_cast<bool>(metrics) && IsBmpUtf8(source);
        const auto* input = reinterpret_cast<const unsigned char*>(source);
        while (*input) {
            if (unicode) {
                const auto decoded = legacy_text::detail::DecodeUtf8(input);
                cells.push_back(static_cast<int>(decoded.scalar));
                input += decoded.consumed;
            } else {
                cells.push_back(*input++);
            }
        }
    }

    int VisibleLength() const {
        return static_cast<int>(std::count_if(cells.begin(), cells.end(), [](int character) {
            return character != '\n';
        }));
    }
};

} // namespace hud_message
} // namespace cse

#endif
