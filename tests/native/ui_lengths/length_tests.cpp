#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <map>
#include <string>
#include <vector>

#include "../../../src/vgui2/src/public/UtlBuffer.h"

#define REQUIRE(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition); return 1; \
} } while (false)

struct Surface {
    void GetCharABCwide(int, wchar_t, int& a, int& b, int& c) {
        a = 1;
        b = 2;
        c = 3;
    }
    int GetFontTall(int) {
        return 12;
    }
};

struct Localize {
    std::map<std::string, std::wstring> strings;
    wchar_t* Find(const char* name) {
        const auto found = strings.find(name);
        return found == strings.end() ? nullptr : &found->second[0];
    }
    void ConvertANSIToUnicode(const char* input, wchar_t* output, int bytes) {
        const size_t capacity = static_cast<size_t>(bytes) / sizeof(wchar_t);
        const size_t length = std::min(std::strlen(input), capacity - 1);
        for (size_t i = 0; i < length; ++i) {
            output[i] = static_cast<unsigned char>(input[i]);
        }
        output[length] = 0;
    }
    void ConvertUnicodeToANSI(const wchar_t* input, char* output, int bytes) {
        const size_t length = std::min(std::wcslen(input), static_cast<size_t>(bytes - 1));
        for (size_t i = 0; i < length; ++i) {
            output[i] = static_cast<char>(input[i]);
        }
        output[length] = 0;
    }
};

Surface testSurface;
Localize testLocalize;
namespace vgui2 {
Surface* surface() {
    return &testSurface;
}
Localize* localize() {
    return &testLocalize;
}
}

struct CHudVGUI2Print {
    int m_hudfont = 1;
    void GetStringSize(const wchar_t* text, int* width, int* height);
};
struct Player { const char* name = nullptr; };
Player g_PlayerInfoList[2];
#include "hud_bodies.inc"

int main(int argc, char** argv) {
    REQUIRE(argc == 2);
    const std::string test = argv[1];
    if (test == "basename") {
        const std::pair<const char*, const char*> cases[] = {
            {"maps/de_dust2.bsp", "de_dust2"}, {"C:\\cstrike\\liblist.gam", "liblist"},
            {"maps.v1/de_map", "de_map"}, {"relative", "relative"}, {"file.a.b", "file.a"},
            {"", ""}, {"/", ""}, {".hidden", ""}, {"a", "a"}
        };
        for (const auto& item : cases) {
            char output[64];
            std::memset(output, 0x5A, sizeof(output));
            COM_FileBase(item.first, output);
            REQUIRE(std::strcmp(output, item.second) == 0);
            REQUIRE(output[std::strlen(item.second) + 1] == 0x5A);
        }
        const std::string large(100000, 'x');
        std::vector<char> output(large.size() + 2, 'z');
        COM_FileBase(large.c_str(), output.data());
        REQUIRE(std::strcmp(output.data(), large.c_str()) == 0 && output.back() == 'z');
    } else if (test == "font-length") {
        CHudVGUI2Print hud;
        int width = -1;
        int height = -1;
        const std::wstring text(100000, L'x');
        hud.GetStringSize(text.c_str(), &width, &height);
        REQUIRE(width == 600000 && height == 12);
        hud.GetStringSize(L"", &width, nullptr);
        REQUIRE(width == 0);
        hud.m_hudfont = 0;
        hud.GetStringSize(L"abc", &width, &height);
        REQUIRE(width == 0 && height == 0);
    } else if (test == "spectator-name") {
        for (size_t length : {size_t(0), size_t(1), size_t(255), size_t(256), size_t(100000)}) {
            const std::string name(length, 'x');
            g_PlayerInfoList[1].name = name.c_str();
            REQUIRE(SpectatorOffset() == static_cast<int>(std::min(length, size_t(255))) * 3);
        }
    } else if (test == "chat-bounds") {
        REQUIRE(PrepareChat("hello\r\n\n") == L"hello");
        REQUIRE(PrepareChat("\r\n") == L"");
        REQUIRE(PrepareChat(std::string(100000, '\n').c_str()).empty());
        REQUIRE(PrepareChat((std::string(100000, 'x') + "\r\n").c_str()) == std::wstring(255, L'x'));
        testLocalize.strings["#t"] = L"word";
        REQUIRE(PrepareChat("a #t z") == L"a word z");
        REQUIRE(PrepareChat("#t") == L"word");
        testLocalize.strings["#t"] = std::wstring(100000, L'y');
        REQUIRE(PrepareChat("a #t z") == L"a " + std::wstring(253, L'y'));
        const std::string token = "#" + std::string(254, 'x');
        REQUIRE(PrepareChat(token.c_str()) == L"#" + std::wstring(126, L'x'));
    } else if (test == "buffer-envelope") {
        unsigned char bytes[8];
        std::memset(bytes, 0xA5, sizeof(bytes));
        CUtlBuffer binary(bytes + 1, 4, false);
        binary.PutString("abc");
        REQUIRE(binary.IsValid() && binary.TellPut() == 4);
        REQUIRE(std::memcmp(bytes + 1, "abc\0", 4) == 0 && bytes[0] == 0xA5 && bytes[5] == 0xA5);
        char value[8] = {};
        binary.GetString(value);
        REQUIRE(std::strcmp(value, "abc") == 0);
        binary.PutString("");
        REQUIRE(!binary.IsValid() && bytes[5] == 0xA5);
        CUtlBuffer text(bytes + 1, 3, true);
        text.PutString("xyz");
        REQUIRE(text.IsValid() && text.TellPut() == 3 && bytes[4] == 0);
        CUtlBuffer unterminated(bytes + 1, 3, false);
        unterminated.GetString(value);
        REQUIRE(!unterminated.IsValid() && value[0] == 0);
        CUtlBuffer growing;
        growing.PutString("a");
        growing.PutString("bc");
        REQUIRE(growing.IsValid() && growing.TellPut() == 5);
        REQUIRE(std::memcmp(growing.Base(), "a\0bc\0", 5) == 0);
    } else if (test == "buffer-counts") {
        unsigned char bytes[8] = {};
        for (bool text : {false, true}) {
            CUtlBuffer full(bytes, sizeof(bytes), text);
            full.SeekPut(CUtlBuffer::SEEK_HEAD, INT_MAX);
            full.PutString("x");
            REQUIRE(!full.IsValid() && full.TellPut() == INT_MAX && bytes[0] == 0);
            CUtlBuffer negative(bytes, sizeof(bytes), text);
            negative.SeekPut(CUtlBuffer::SEEK_HEAD, -1);
            negative.PutString("x");
            REQUIRE(!negative.IsValid() && bytes[0] == 0);
        }
        CUtlBuffer full(bytes, sizeof(bytes), false);
        full.SeekPut(CUtlBuffer::SEEK_HEAD, INT_MAX);
        full.PutString("");
        REQUIRE(!full.IsValid());
        CUtlBuffer terminator(bytes, sizeof(bytes), false);
        terminator.SeekPut(CUtlBuffer::SEEK_HEAD, INT_MAX - 1);
        terminator.PutString("x");
        REQUIRE(!terminator.IsValid());
        CUtlBuffer negativeCount(bytes, sizeof(bytes), false);
        negativeCount.Put(bytes, -1);
        REQUIRE(!negativeCount.IsValid() && negativeCount.TellPut() == 0);
        CUtlBuffer rawFull(bytes, sizeof(bytes), false);
        rawFull.SeekPut(CUtlBuffer::SEEK_HEAD, INT_MAX);
        rawFull.Put(bytes, 1);
        REQUIRE(!rawFull.IsValid());
        CUtlBuffer readFull(bytes, sizeof(bytes), false);
        readFull.SeekGet(CUtlBuffer::SEEK_HEAD, INT_MAX);
        char value[4] = {'x', 0};
        readFull.GetString(value);
        REQUIRE(!readFull.IsValid() && value[0] == 0);
    } else {
        REQUIRE(false);
    }
    return 0;
}
