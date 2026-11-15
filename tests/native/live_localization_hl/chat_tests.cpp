#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "localization_client.h"
#include "legacy_text_encoding.h"
constexpr int MAX_LINES = 5;
constexpr int MAX_CHARS_PER_LINE = 256;
constexpr int ScreenWidth = 160;
constexpr int ScreenHeight = 480;
constexpr int LINE_START = 10;
constexpr int MAX_LINE_WIDTH = ScreenWidth - 40;
char g_szLineBuffer[MAX_LINES + 1][MAX_CHARS_PER_LINE]{};
float* g_pflNameColors[MAX_LINES + 1]{};
int g_iNameLengths[MAX_LINES + 1]{};
std::shared_ptr<HudTextSource> g_lineSources[MAX_LINES + 1];
int line_height = 12;
int Y_START = 0;
float flScrollTime = 99.0f;
int consolePrints = 0;
std::map<std::string, std::string> titles;
struct Message { const char* pMessage; } record;
Message* TextMessageGet(const char* key) {
    auto found = titles.find(key);
    if (found == titles.end()) { return nullptr; }
    record.pMessage = found->second.c_str();
    return &record;
}
bool IsCatalogueMessage(const Message* message) { return message == &record; }
void ConsolePrint(const char*) { ++consolePrints; }
int safe_sprintf(char* output, int capacity, const char* format, ...) {
    va_list args;
    va_start(args, format);
    const int result = vsnprintf(output, capacity, format, args);
    va_end(args);
    return result;
}
void GetConsoleStringSize(const char* text, int* wide, int* tall) {
    *wide = 0;
    *tall = 12;
    for (std::size_t offset = 0; text[offset];) {
        const auto character = cse::legacy_text::detail::DecodeUtf8(
            reinterpret_cast<const unsigned char*>(text + offset));
        offset += character.consumed;
        *wide += 8;
    }
}
struct CHudSayText {
    int m_iFlags = 3;
    void RefreshLanguage();
};
#include "chat.inc"
int main() {
    CHudSayText hud;
    auto source = std::make_shared<HudTextSource>();
    titles["FORMAT"] = "English %s";
    std::string raw[5]{"#FORMAT", "#player", "", "", ""};
    const char* resolved[5]{"English %s", "#player", "", "", ""};
    assert(source->Capture(raw, resolved));
    strcpy(g_szLineBuffer[0], "literal #FORMAT");
    float color = 1;
    g_pflNameColors[0] = &color;
    g_iNameLengths[0] = 7;
    strcpy(g_szLineBuffer[1], "English #player");
    g_lineSources[1] = source;
    titles["FORMAT"] = "Русский %s";
    titles["player"] = "should remain literal";
    hud.RefreshLanguage();
    assert(strcmp(g_szLineBuffer[0], "literal #FORMAT") == 0);
    assert(g_pflNameColors[0] == &color && g_iNameLengths[0] == 7);
    std::string combined;
    for (int row = 1; row < MAX_LINES; ++row) { combined += g_szLineBuffer[row]; }
    assert(combined == "Русский #player");
    assert(g_lineSources[1] == source && g_lineSources[2] == source);
    assert(flScrollTime == 99.0f && hud.m_iFlags == 3 && consolePrints == 0);
    hud.RefreshLanguage();
    assert(flScrollTime == 99.0f && hud.m_iFlags == 3 && consolePrints == 0);
    ScrollTextUp();
    ScrollTextUp();
    hud.RefreshLanguage();
    assert(g_szLineBuffer[0][0] == 0); // Discarded logical messages never reappear.
    assert(flScrollTime == 99.0f && hud.m_iFlags == 3);
    for (int row = 0; row <= MAX_LINES; ++row) {
        g_szLineBuffer[row][0] = 0;
        g_lineSources[row].reset();
    }
    auto voice = std::make_shared<HudTextSource>();
    voice->presentation = HudTextSource::Presentation::VoiceStatus;
    titles["Unmuted"] = "U %s.\r";
    titles["M"] = "must stay literal";
    const std::string voiceRaw[5]{"#Unmuted", "", "", "", ""};
    const char* voiceResolved[]{"U %s.\r", "#M", "", "", ""};
    assert(voice->Capture(voiceRaw, voiceResolved));
    strcpy(g_szLineBuffer[0], voice->Render().c_str());
    g_lineSources[0] = voice;
    titles["Unmuted"] = "P %s.\r";
    const int priorConsolePrints = consolePrints;
    hud.RefreshLanguage();
    assert(strcmp(g_szLineBuffer[0], "** P #M.\n\n") == 0);
    assert(flScrollTime == 99.0f && hud.m_iFlags == 3 && consolePrints == priorConsolePrints);
    puts("PASS actual HL talk owner: typed reflow, literal chat/name metadata, scroll time and no event replay");
}
