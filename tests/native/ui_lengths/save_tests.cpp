#include <algorithm>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <string>
#include <vector>

#include "../../../src/common/legacy_text_encoding.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#define REQUIRE(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition); return 1; \
} } while (false)

using FileHandle_t = void*;
using StringIndex_t = size_t;
using ucs2 = unsigned short;
constexpr StringIndex_t INVALID_LOCALIZE_STRING_INDEX = static_cast<StringIndex_t>(-1);

struct IFileSystem {
    std::vector<unsigned char> bytes;
    int writes = 0;
    int maximumWrite = 0;
    int shortWriteAt = -1;
    bool openFails = false;
    bool closed = false;
    FileHandle_t Open(const char*, const char*, const char*) {
        return openFails ? nullptr : this;
    }
    int Write(const void* input, int count, FileHandle_t) {
        maximumWrite = std::max(maximumWrite, count);
        if (writes++ == shortWriteAt) {
            return 0;
        }
        const auto* begin = static_cast<const unsigned char*>(input);
        bytes.insert(bytes.end(), begin, begin + count);
        return count;
    }
    void Close(FileHandle_t) {
        closed = true;
    }
};
struct CUtlSymbol {
    std::string name;
    explicit CUtlSymbol(const char* text = "") : name(text) {}
    bool operator==(const CUtlSymbol& other) const {
        return name == other.name;
    }
};
struct Entry {
    CUtlSymbol filename;
    std::string name;
    std::wstring value;
};
struct CLocalizedStringTable {
    std::vector<Entry> m_Lookup;
    int ConvertANSIToUnicode(const char* input, wchar_t* output, int bytes);
    bool SaveToFile(IFileSystem* fileSystem, const char* name);
    StringIndex_t GetFirstStringIndex() {
        return m_Lookup.empty() ? INVALID_LOCALIZE_STRING_INDEX : 0;
    }
    StringIndex_t GetNextStringIndex(StringIndex_t index) {
        return index + 1 < m_Lookup.size() ? index + 1 : INVALID_LOCALIZE_STRING_INDEX;
    }
    const char* GetNameByIndex(StringIndex_t index) {
        return m_Lookup[index].name.c_str();
    }
    wchar_t* GetValueByIndex(StringIndex_t index) {
        return &m_Lookup[index].value[0];
    }
};
#include "localization_bodies.inc"

std::vector<unsigned char> UTF16Bytes(const std::u16string& value) {
    std::vector<unsigned char> result = {0xFF, 0xFE};
    for (char16_t unit : value) {
        result.push_back(static_cast<unsigned char>(unit & 0xFF));
        result.push_back(static_cast<unsigned char>(unit >> 8));
    }
    return result;
}

int main(int argc, char** argv) {
    REQUIRE(argc == 2);
    std::setlocale(LC_ALL, "en_US.UTF-8");
    const std::string test = argv[1];
    CLocalizedStringTable table;
    const std::u16string start = u"\"lang\"\r\n{\r\n\"Language\" \"English\"\r\n\"Tokens\"\r\n{\r\n";
    std::u16string end = u"}\r\n}\r\n";
    end.push_back(0);
    if (test == "localization-bytes") {
        table.m_Lookup.push_back({CUtlSymbol("test.txt"), "Token", L"ASCII \u0416"});
        table.m_Lookup.push_back({CUtlSymbol("other.txt"), "Skipped", L"ignored"});
        table.m_Lookup.push_back({CUtlSymbol("test.txt"), "Empty", L""});
        IFileSystem file;
        REQUIRE(table.SaveToFile(&file, "test.txt") && file.closed);
        REQUIRE(file.bytes == UTF16Bytes(start + u"\t\"Token\"\t\t\"ASCII \u0416\"\r\n\t\"Empty\"\t\t\"\"\r\n" + end));
        std::vector<ucs2> units;
        for (size_t i = 2; i < file.bytes.size(); i += 2) {
            units.push_back(static_cast<ucs2>(file.bytes[i] | (file.bytes[i + 1] << 8)));
        }
        auto* cursor = units.data();
        bool quoted = false;
        ucs2 token[2049];
        std::vector<std::u16string> tokens;
        while (*cursor) {
            cursor = ReadUnicodeToken(cursor, token, 2048, quoted);
            std::u16string value;
            for (const ucs2* scan = token; *scan; ++scan) {
                value.push_back(static_cast<char16_t>(*scan));
            }
            tokens.push_back(value);
        }
        REQUIRE(std::find(tokens.begin(), tokens.end(), u"ASCII \u0416") != tokens.end());
    } else if (test == "localization-supplementary") {
        std::wstring units;
        units.push_back(static_cast<wchar_t>(0xD83D));
        units.push_back(static_cast<wchar_t>(0xDE42));
        table.m_Lookup.push_back({CUtlSymbol("test.txt"), "Surrogates", units});
        table.m_Lookup.push_back({CUtlSymbol("test.txt"), "Scalar", L"\U0001F642"});
        IFileSystem file;
        REQUIRE(table.SaveToFile(&file, "test.txt") && file.closed);
        REQUIRE(file.bytes == UTF16Bytes(start + u"\t\"Surrogates\"\t\t\"\U0001F642\"\r\n\t\"Scalar\"\t\t\"\U0001F642\"\r\n" + end));
    } else if (test == "localization-chunks") {
        table.m_Lookup.push_back({CUtlSymbol("test.txt"), "Large", std::wstring(100000, L'x')});
        IFileSystem file;
        REQUIRE(table.SaveToFile(&file, "test.txt") && file.closed);
        REQUIRE(file.bytes == UTF16Bytes(start + u"\t\"Large\"\t\t\"" + std::u16string(100000, u'x') + u"\"\r\n" + end));
        REQUIRE(file.maximumWrite <= 1024 && file.writes > 190);
    } else if (test == "localization-failures") {
        IFileSystem missing;
        missing.openFails = true;
        REQUIRE(!table.SaveToFile(&missing, "test.txt") && !missing.closed && missing.writes == 0);
        for (int write = 0; write < 3; ++write) {
            IFileSystem shortWrite;
            shortWrite.shortWriteAt = write;
            REQUIRE(!table.SaveToFile(&shortWrite, "test.txt") && shortWrite.closed);
            REQUIRE(shortWrite.writes == write + 1);
        }
        if (sizeof(wchar_t) > 2) {
            std::wstring invalid(1, static_cast<wchar_t>(0x110000));
            table.m_Lookup.push_back({CUtlSymbol("test.txt"), "Invalid", invalid});
            IFileSystem file;
            REQUIRE(!table.SaveToFile(&file, "test.txt") && file.closed);
        }
    } else {
        REQUIRE(false);
    }
    return 0;
}
