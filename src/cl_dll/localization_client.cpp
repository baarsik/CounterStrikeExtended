#include "hud.h"
#include "cl_util.h"
#include "localization_client.h"
#include "native_module.h"
#include "../common/legacy_text_encoding.h"
#include <cstring>
#include "vgui_TeamFortressViewport.h"

const cse::localization::EngineApi* GetLocalizationEngine() {
    if (!gEngfuncs.pfnTextMessageGet) {
        return nullptr;
    }
    void* module = nullptr;
#if defined(_WIN32)
    HMODULE owner = nullptr;
    constexpr DWORD OwnerFlags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
    if (!GetModuleHandleExA(OwnerFlags, reinterpret_cast<LPCSTR>(gEngfuncs.pfnTextMessageGet), &owner)) {
        return nullptr;
    }
    module = owner;
#else
    Dl_info owner{};
    if (!dladdr(reinterpret_cast<const void*>(gEngfuncs.pfnTextMessageGet), &owner) || !owner.dli_fname) {
        return nullptr;
    }
    module = cse_platform::FindLoadedModule(owner.dli_fname);
#endif
    const auto factory = reinterpret_cast<CreateInterfaceFn>(
        cse_platform::GetSymbol(module, cse_platform::InterfaceFactorySymbol));
    return cse::localization::QueryEngine(factory);
}

bool IsCatalogueMessage(const client_textmessage_s* message) {
    const auto* engine = GetLocalizationEngine();
    return engine && engine->IsCatalogueMessage(message);
}

void RefreshClientLanguage() {
    static std::uint64_t previousRevision = 0;
    const auto* engine = GetLocalizationEngine();
    if (!engine || engine->Revision() == previousRevision) {
        return;
    }
    previousRevision = engine->Revision();
    gHUD.m_Message.RefreshLanguage();
    gHUD.m_TextMessage.RefreshLanguage();
    gHUD.m_SayText.RefreshLanguage();
    gHUD.m_Menu.RefreshLanguage();
    gHUD.m_StatusBar.RefreshLanguage();
    if (gViewPort) {
        gViewPort->RefreshLanguage(engine->Language());
    }
}

bool HudTextSource::Capture(const std::string* raw, const char* const* resolved) {
    bool hasToken = false;
    for (int index = 0; index < 5; ++index) {
        tokens[index] = raw[index].size() > 1 && raw[index][0] == '#'
            && IsCatalogueMessage(TextMessageGet(raw[index].c_str() + 1));
        values[index] = tokens[index] ? raw[index].substr(1) : resolved[index];
        hasToken = hasToken || tokens[index];
    }
    return hasToken;
}

std::string HudTextSource::Render() const {
    constexpr int VoiceExplanationIndex = 2;
    std::string expanded[5];
    for (int index = 0; index < 5; ++index) {
        expanded[index] = values[index];
        if (tokens[index]) {
            const auto* message = TextMessageGet(values[index].c_str());
            if (!message || !message->pMessage) {
                return {};
            }
            expanded[index] = message->pMessage;
        }
        const bool completeVoiceTitle = presentation == Presentation::VoiceStatus && index == VoiceExplanationIndex;
        if (index > 0 && tokens[index] && !completeVoiceTitle && !expanded[index].empty()
            && (expanded[index].back() == '\n' || expanded[index].back() == '\r')) {
            expanded[index].pop_back();
        }
    }
    constexpr int TextMessageBytes = 128;
    char text[TextMessageBytes];
    if (presentation == Presentation::VoiceStatus) {
        char status[TextMessageBytes];
        safe_sprintf(status, sizeof(status), expanded[0].c_str(), expanded[1].c_str());
        if (expanded[VoiceExplanationIndex].empty()) {
            safe_sprintf(text, sizeof(text), "** %s\n", status);
        } else {
            safe_sprintf(text, sizeof(text), "** %s %s\n", status, expanded[VoiceExplanationIndex].c_str());
        }
    } else {
        safe_sprintf(text, sizeof(text), expanded[0].c_str(), expanded[1].c_str(), expanded[2].c_str(),
            expanded[3].c_str(), expanded[4].c_str());
    }
    const char* sources[]{expanded[0].c_str(), expanded[1].c_str(), expanded[2].c_str(),
        expanded[3].c_str(), expanded[4].c_str()};
    FinishHudText(text, sources);
    for (char* character = text; *character; ++character) {
        if (*character == '\r') {
            *character = '\n';
        }
    }
    return text;
}

void FinishHudText(char* rendered, const char* const* sources) {
    const auto validPrefix = [](const char* text) {
        std::size_t offset = 0;
        while (text[offset]) {
            const auto* input = reinterpret_cast<const unsigned char*>(text + offset);
            const auto decoded = cse::legacy_text::detail::DecodeUtf8(input);
            const auto encoded = cse::legacy_text::detail::EncodeUtf8(decoded.scalar);
            if (decoded.consumed != encoded.count || std::memcmp(input, encoded.bytes, encoded.count) != 0) {
                break;
            }
            offset += decoded.consumed;
        }
        return offset;
    };
    for (int index = 0; index < 5; ++index) {
        if (validPrefix(sources[index]) != std::strlen(sources[index])) {
            return; // Preserve retained CP1252 byte semantics.
        }
    }
    rendered[validPrefix(rendered)] = 0;
}
