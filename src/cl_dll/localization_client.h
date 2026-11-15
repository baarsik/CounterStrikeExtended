#ifndef CSE_HL_LOCALIZATION_CLIENT_H
#define CSE_HL_LOCALIZATION_CLIENT_H

#include "localization_service.h"
#include <string>

struct HudTextSource {
    enum class Presentation { TextMessage, VoiceStatus };

    std::string values[5];
    bool tokens[5]{};
    int discardedLines = 0;
    Presentation presentation = Presentation::TextMessage;
    bool Capture(const std::string* raw, const char* const* resolved);
    std::string Render() const;
};

const cse::localization::EngineApi* GetLocalizationEngine();
bool IsCatalogueMessage(const client_textmessage_s* message);
void RefreshClientLanguage();
void FinishHudText(char* rendered, const char* const* sources);

#endif
