// Complete actual Con_DrawNotify body, with named draw/time/global service seams.
// Missing NUL/bounds checks must fail visible-prefix or full-buffer cases.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "notify_global.inc"
float notifyTimes[4];
float* con_times = notifyTimes;
int con_current;
int con_num_times;
int con_linewidth;
double realtime;
struct NotifyTime { float value; } con_notifytime;
int clearnotify;
int scr_copytop;
int con_notifylines;
int key_dest;
constexpr int key_message = 1;
struct Video { int width; } vid;
const char* chat_buffer;
int chat_bufferlen;
const char* message_type;
float con_cursorspeed;

namespace {
constexpr unsigned int kConsoleFont = 42;
constexpr int kFontTall = 18;
constexpr int kGlyphWidth = 7;
constexpr int kInitialX = 8;
struct Glyph {
    int x;
    int y;
    int character;
    unsigned int font;
};
struct Text {
    int x;
    int y;
    std::string value;
};
std::vector<Glyph> glyphs;
std::vector<Text> strings;
std::vector<std::string> services;
int fontTallCalls;
int chatPositionCalls;
int advanceCount;
const char* currentCase;

void Require(bool condition, const char* reason) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s: %s (glyphs=%zu lines=%d clear=%d copy=%d)\n",
            currentCase, reason, glyphs.size(), con_notifylines, clearnotify, scr_copytop);
        std::exit(1);
    }
}
}

void Con_DrawDebugAreas() {
    services.push_back("debug");
}

void Draw_ResetTextColor() {
    services.push_back("reset-color");
}

unsigned int VGUI2_GetConsoleFont() {
    return kConsoleFont;
}

int VGUI2_GetFontTall(unsigned int font) {
    Require(font == kConsoleFont, "font-height font unchanged");
    ++fontTallCalls;
    return kFontTall;
}

int Draw_Character(int x, int y, int character, unsigned int font) {
    glyphs.push_back({x, y, character, font});
    return kGlyphWidth;
}

void ClientDLL_ChatInputPosition(int* x, int* y) {
    ++chatPositionCalls;
    *x = 13;
    *y += 6;
}

const char* Q_UnicodeAdvance(const char* text, int count) {
    advanceCount = count;
    return text + count;
}

int Draw_String(int x, int y, const char* text) {
    strings.push_back({x, y, text});
    return x + static_cast<int>(std::strlen(text)) * 5;
}

#include "notify.inc"

namespace {
void Reset() {
    std::fill(std::begin(g_szNotifyAreaString), std::end(g_szNotifyAreaString), 'Z');
    std::fill(std::begin(notifyTimes), std::end(notifyTimes), 0.0f);
    notifyTimes[0] = 1.0f;
    con_current = 0;
    con_num_times = 1;
    con_linewidth = 318;
    realtime = 2.0;
    con_notifytime.value = 4.0f;
    clearnotify = 9;
    scr_copytop = 0;
    con_notifylines = 0;
    key_dest = 0;
    vid.width = 2560;
    chat_buffer = "abcdef";
    chat_bufferlen = 6;
    message_type = "say";
    con_cursorspeed = 4.0f;
    glyphs.clear();
    strings.clear();
    services.clear();
    fontTallCalls = 0;
    chatPositionCalls = 0;
    advanceCount = -1;
}

void CheckPrefix(const std::string& prefix, int rows, int extent) {
    Require(glyphs.size() == prefix.size() * static_cast<std::size_t>(rows), "only visible-prefix draws");
    for (int row = 0; row < rows; ++row) {
        for (std::size_t index = 0; index < prefix.size(); ++index) {
            const Glyph& glyph = glyphs[row * prefix.size() + index];
            Require(glyph.character == static_cast<char>(prefix[index]), "character bytes unchanged");
            Require(glyph.font == kConsoleFont, "console font unchanged");
            Require(glyph.x == kInitialX + static_cast<int>(index) * kGlyphWidth, "glyph width accumulation");
            Require(glyph.y == row * kFontTall, "line position unchanged");
        }
    }
    Require(fontTallCalls == rows, "one height update per live notification");
    Require(con_notifylines == extent, "notify line bookkeeping");
    Require(clearnotify == (rows ? 0 : 9), "clear flag unchanged");
    Require(scr_copytop == (rows ? 1 : 0), "copy flag unchanged");
}

void CheckChat(bool notify) {
    Require(chatPositionCalls == 1 && advanceCount == 3, "independent chat positioning and truncation");
    const int y = notify ? 24 : 6;
    Require(strings.size() == 3, "all chat strings drawn");
    Require(strings[0].x == 13 && strings[0].y == y && strings[0].value == "say", "chat label");
    Require(strings[1].x == 31 && strings[1].y == y && strings[1].value == ":", "chat separator");
    Require(strings[2].x == 39 && strings[2].y == y && strings[2].value == "def", "chat visible text");
    Require(glyphs.size() == (notify ? 3u : 1u), "chat cursor and notify counts");
    const Glyph& cursor = glyphs.back();
    Require(cursor.x == 55 && cursor.y == y && cursor.character == 10 && cursor.font == kConsoleFont,
        "chat cursor unchanged");
    Require(clearnotify == 0 && scr_copytop == 1, "chat flags");
    Require(con_notifylines == y + kFontTall, "chat line extent");
    Require(fontTallCalls == (notify ? 2 : 1), "chat and notify font height updates");
    if (notify) {
        Require(glyphs[0].character == 'O' && glyphs[0].x == 8 && glyphs[0].y == 0
            && glyphs[0].font == kConsoleFont, "notify before chat first glyph");
        Require(glyphs[1].character == 'K' && glyphs[1].x == 15 && glyphs[1].y == 0
            && glyphs[1].font == kConsoleFont, "notify before chat second glyph");
    }
}
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: console_notify_tests CASE\n");
        return 2;
    }
    currentCase = argv[1];
    const std::string name = currentCase;
    Reset();
    std::string expected = "OK";
    int rows = 1;
    std::memcpy(g_szNotifyAreaString, "OK", 3);
    if (name == "short-1920" || name == "full-1920") {
        vid.width = 1920;
        con_linewidth = 238;
    } else if (name == "short-3840" || name == "full-3840") {
        vid.width = 3840;
        con_linewidth = 478;
    } else if (name == "short-stale") {
        con_linewidth = 238;
    } else if (name == "short-2560" || name == "full-2560") {
        // Default: 2560 pixels, 318 console columns; stale suffix remains after NUL.
    } else if (name == "empty-stale") {
        g_szNotifyAreaString[0] = '\0';
        expected.clear();
    } else if (name == "columns-narrow") {
        con_linewidth = 1;
        expected = "O";
    } else if (name == "columns-zero" || name == "columns-negative") {
        con_linewidth = name == "columns-zero" ? 0 : -7;
        expected.clear();
    } else if (name == "high-bit") {
        g_szNotifyAreaString[0] = static_cast<char>(0x80);
        g_szNotifyAreaString[1] = static_cast<char>(0xff);
        g_szNotifyAreaString[2] = '\0';
        expected.assign(g_szNotifyAreaString, 2);
    } else if (name == "expired" || name == "chat-expired") {
        realtime = 6.0;
        rows = 0;
    } else if (name == "inactive") {
        notifyTimes[0] = 0.0f;
        rows = 0;
    } else if (name == "timing-boundary") {
        realtime = 5.0;
    } else if (name == "timing-ring") {
        con_current = 4;
        con_num_times = 4;
        notifyTimes[0] = 2.0f;
        notifyTimes[1] = 0.0f;
        notifyTimes[2] = -3.0f;
        notifyTimes[3] = 1.0f;
        rows = 2;
    } else if (name == "negative-history") {
        con_current = 1;
        con_num_times = 4;
        notifyTimes[1] = 2.0f;
        rows = 2;
    } else if (name == "line-maximum") {
        con_notifylines = 90;
    } else if (name != "chat-after-notify") {
        Require(false, "known case");
    }
    if (name.compare(0, 5, "full-") == 0) {
        std::fill(std::begin(g_szNotifyAreaString), std::end(g_szNotifyAreaString), 'F');
        expected.assign(name == "full-1920" ? 238 : 256, 'F');
    }
    const bool chat = name == "chat-expired" || name == "chat-after-notify";
    if (chat) {
        key_dest = key_message;
        vid.width = 30;
        realtime = name == "chat-expired" ? 6.0 : 2.0;
    }
    const std::string before(g_szNotifyAreaString, sizeof(g_szNotifyAreaString));
    Con_DrawNotify();
    Require(services == std::vector<std::string>({"debug", "reset-color"}), "debug/reset always called in order");
    Require(before == std::string(g_szNotifyAreaString, sizeof(g_szNotifyAreaString)), "notify storage unchanged");
    if (chat) {
        CheckChat(name == "chat-after-notify");
    } else {
        CheckPrefix(expected, rows, name == "line-maximum" ? 90 : rows * kFontTall);
        Require(strings.empty() && chatPositionCalls == 0 && advanceCount == -1, "inactive chat untouched");
    }
    std::printf("PASS %s\n", currentCase);
    return 0;
}
