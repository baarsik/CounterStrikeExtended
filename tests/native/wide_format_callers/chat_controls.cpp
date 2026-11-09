#include "legacy_wide_format.h"
#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <cwchar>
#include <cwctype>
#include <string>

#define ARRAYSIZE(array) (sizeof(array) / sizeof((array)[0]))
// This binding exists only for extraction: the pre-fix native branches still
// call libc directly and therefore cannot be hidden by this macro.
#define _snwprintf CseLegacySnwprintf
#define _vsnwprintf CseLegacyVsnwprintf

namespace {
constexpr std::size_t kChatCells = 256;
constexpr int kArgumentCount = 4;
constexpr int kRepeatedCalls = 3;
constexpr unsigned int kAsciiLimit = 128;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s\n", message);
        std::abort();
    }
}

class ControlledLocalize {
public:
    int constructed = 0;
    wchar_t* voiceFormat = nullptr;
    wchar_t* Find(const char*) {
        return voiceFormat;
    }

    void ConvertANSIToUnicode(const char* text, wchar_t* output, int bytes) {
        const std::size_t cells = static_cast<std::size_t>(bytes) / sizeof(wchar_t);
        std::size_t index = 0;
        while (text[index] && index + 1 < cells) {
            Require(static_cast<unsigned char>(text[index]) < kAsciiLimit, "owned HUD format is ASCII");
            output[index] = static_cast<unsigned char>(text[index]);
            ++index;
        }
        output[index] = 0;
    }

    void ConstructString(wchar_t* output, int bytes, const wchar_t* format, int count, ...) {
        ++constructed;
        Require(count == kArgumentCount && std::wcscmp(format, L"%s1") == 0, "controlled Valve placeholder dispatch");
        va_list arguments;
        va_start(arguments, count);
        const wchar_t* first = va_arg(arguments, const wchar_t*);
        va_end(arguments);
        const std::size_t cells = static_cast<std::size_t>(bytes) / sizeof(wchar_t);
        std::wcsncpy(output, first, cells - 1);
        output[cells - 1] = 0;
    }
};
ControlledLocalize localizer;
}
namespace vgui2 {
ControlledLocalize* localize() {
    return &localizer;
}
}

class HudFormattingFixture {
public:
    static constexpr std::size_t kCells = 512;
    wchar_t m_wCharBuf[kCells]{};
    void Format(const char* text, const wchar_t* first, const wchar_t* second,
        const wchar_t* third, const wchar_t* fourth) {
#include "actual_hud.inc"
    }
};

void ChatFormattingFixture(wchar_t (&finalBuffer)[kChatCells], wchar_t* localized, wchar_t* (&unicodeArgs)[kArgumentCount]) {
#include "actual_chat.inc"
}

class ControlledLabel {
public:
    std::wstring text;
    void SetText(const wchar_t* value) {
        text = value;
    }
};
class CVoiceLabel {
public:
    char* m_playerName = nullptr;
    wchar_t* m_locationString = nullptr;
    ControlledLabel* m_pLabel = nullptr;
    void RebuildLabelText();
};
#include "actual_voice.inc"

void Numeric64(wchar_t (&text)[64], int value) {
#include "actual_numeric64.inc"
}
void Numeric16(wchar_t (&repeat)[16], int m_repeat) {
#include "actual_numeric16.inc"
}

namespace actual_wrappers {
#include "actual_wrappers.h"
}

int main() {
    wchar_t first[]{L'A', 0xd83d, 0xde42, L'Z', 0};
    wchar_t second[]{0x00e9, L'B', 0};
    wchar_t empty[]{0};
    wchar_t* arguments[]{first, second, empty, empty};
    // A real mutable resource copy lets the RED prove mutation without invoking
    // a write to read-only memory. The final branch also accepts const-backed data.
    wchar_t format[]{L'%', L'.', L'3', L's', L'/', L'%', L's', 0};
    const std::wstring original(format);
    wchar_t result[kChatCells]{};
    for (int repeat = 0; repeat < kRepeatedCalls; ++repeat) {
        ChatFormattingFixture(result, format, arguments);
        Require(original == format, "actual chat branch must not mutate a localized resource");
        Require(std::wcscmp(result, L"A\xd83d\xde42/\x00e9" L"B") == 0,
            "actual chat branch preserves canonical wide strings and UTF16 precision");
    }
    auto* constantFormat = const_cast<wchar_t*>(L"%.2s/%s");
    ChatFormattingFixture(result, constantFormat, arguments);
    Require(std::wcscmp(result, L"A\xd83d/\x00e9" L"B") == 0,
        "const-backed chat format remains unmodified and wide precision may split UTF16 pair");
    wchar_t placeholders[]{L'%', L's', L'1', 0};
    ChatFormattingFixture(result, placeholders, arguments);
    Require(localizer.constructed == 1 && std::wcscmp(result, first) == 0,
        "existing Valve numbered placeholders retain ConstructString route");
    HudFormattingFixture hud;
    for (int repeat = 0; repeat < kRepeatedCalls; ++repeat) {
        hud.Format("%.3s/%s", first, second, empty, empty);
        Require(std::wcscmp(hud.m_wCharBuf, result) != 0
            && std::wcscmp(hud.m_wCharBuf, L"A\xd83d\xde42/\x00e9" L"B") == 0,
            "actual HUD branch uses the same canonical format repeatedly");
    }
    const std::wstring longFormat(HudFormattingFixture::kCells + kArgumentCount, L'X');
    std::string narrowFormat(longFormat.size(), 'X');
    hud.Format(narrowFormat.c_str(), first, second, empty, empty);
    for (wchar_t unit : hud.m_wCharBuf) {
        Require(unit == L'X', "actual HUD raw exact-capacity legacy truncation retains all 512 cells");
    }
    int remaining = 4;
    wchar_t append[8]{};
    Require(actual_wrappers::BufWPrintf(append, remaining, L"ABCD") == append + 3
        && remaining == 1 && std::wcscmp(append, L"ABC") == 0,
        "actual shared append wrapper retains its separate NUL/count policy");
    wchar_t sharedFormat[]{L'%', L's', 0};
    Require(std::wcscmp(actual_wrappers::SharedWVarArgs(sharedFormat, first), first) == 0,
        "actual native shared ring wrapper receives wide Windows s");
    ControlledLabel label;
    CVoiceLabel voice;
    char playerName[]{'P', 0};
    voice.m_playerName = playerName;
    voice.m_locationString = second;
    voice.m_pLabel = &label;
    wchar_t voiceFormat[]{L'%', L's', L'/', L'%', L's', 0};
    const std::wstring originalVoice(voiceFormat);
    localizer.voiceFormat = voiceFormat;
    for (int repeat = 0; repeat < kRepeatedCalls; ++repeat) {
        voice.RebuildLabelText();
        Require(label.text == L"P/\x00e9" L"B" && originalVoice == voiceFormat,
            "actual voice label keeps custom Windows wide format and resource ownership");
    }
    localizer.voiceFormat = nullptr;
    voice.RebuildLabelText();
    Require(label.text == L"P/\x00e9" L"B   ", "actual voice fallback ls format remains supported");
    voice.m_locationString = nullptr;
    voice.RebuildLabelText();
    Require(label.text == L"P   ", "actual voice name-only route remains supported");
    wchar_t numeric64[64]{};
    wchar_t numeric16[16]{};
    Numeric64(numeric64, (std::numeric_limits<int>::min)());
    Numeric16(numeric16, (std::numeric_limits<int>::min)());
    Require(std::wcscmp(numeric64, L"-2147483648") == 0 && std::wcscmp(numeric16, numeric64) == 0,
        "actual bounded GameUI standard-call replacements fit signed32 minimum and NUL");
    std::puts("PASS: actual chat/HUD branches, repeated unmodified resources, UTF16 precision and wrapper guards");
    return 0;
}
