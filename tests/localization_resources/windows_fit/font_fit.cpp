// Supplementary Win32 GDI measurement of maintained translation/layout inputs.
// See README.md for scope and the owning VGUI algorithms mirrored here.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

static_assert(sizeof(void*) == 4, "This fixture must be built for Windows x86.");

namespace {
constexpr int MissingGlyph = 0xffff;
constexpr int TextFaceCapacity = LF_FACESIZE;
// Input columns mirror emit_inputs.py's versioned, fixed TSV schema.
constexpr int TypeColumn = 0;
constexpr int LanguageColumn = 1;
constexpr int SourceColumn = 2;
constexpr int ControlColumn = 3;
constexpr int TokenColumn = 4;
constexpr int FaceColumn = 6;
constexpr int TallColumn = 7;
constexpr int WeightColumn = 8;
constexpr int ItalicColumn = 9;
constexpr int UnderlineColumn = 10;
constexpr int StrikeoutColumn = 11;
constexpr int AntialiasColumn = 12;
constexpr int ShadowColumn = 13;
constexpr int WidthColumn = 14;
constexpr int HeightColumn = 15;
constexpr int InsetXColumn = 16;
constexpr int InsetYColumn = 17;
constexpr int IndicatorWidthColumn = 18;
constexpr int IndicatorHeightColumn = 19;
constexpr int WrapColumn = 20;
constexpr int TextColumn = 21;
constexpr int HeaderColumns = TextColumn + 1;
constexpr int LanguageCount = 2;
constexpr int BodyFontsPerLanguage = 6;
constexpr int ExpectedAlphabetChecks = LanguageCount * BodyFontsPerLanguage;

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::vector<std::string> Split(const std::string& value, char delimiter) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    for (;;) {
        const auto end = value.find(delimiter, start);
        parts.push_back(value.substr(start, end - start));
        if (end == std::string::npos) {
            return parts;
        }
        start = end + 1;
    }
}

std::string Unescape(const std::string& value) {
    std::string result;
    for (std::size_t index = 0; index < value.size(); ++index) {
        char character = value[index];
        if (character == '\\' && index + 1 < value.size()) {
            character = value[++index];
            if (character == 'n') {
                character = '\n';
            } else if (character == 'r') {
                character = '\r';
            } else if (character == 't') {
                character = '\t';
            }
        }
        result += character;
    }
    return result;
}

std::wstring Wide(const std::string& value) {
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0);
    Require(size > 0 || value.empty(), "invalid UTF-8 input");
    std::wstring result(size, L'\0');
    if (size > 0) {
        Require(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
            static_cast<int>(value.size()), result.data(), size) == size, "decode UTF-8");
    }
    return result;
}

std::string Utf8(const std::wstring& value) {
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
        nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size,
        nullptr, nullptr);
    return result;
}

bool IsSpace(wchar_t character) {
    WORD type = 0;
    Require(GetStringTypeW(CT_CTYPE1, &character, 1, &type) != 0, "classify Unicode character");
    return (type & C1_SPACE) != 0;
}

bool IsPrintable(wchar_t character) {
    WORD type = 0;
    Require(GetStringTypeW(CT_CTYPE1, &character, 1, &type) != 0, "classify Unicode character");
    return (type & C1_CNTRL) == 0;
}

int CALLBACK FoundFont(const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM parameter) {
    *reinterpret_cast<bool*>(parameter) = true;
    return 0;
}

struct Font {
    HDC dc = nullptr;
    HFONT handle = nullptr;
    HGDIOBJ original = nullptr;
    int height = 0;
    std::wstring face;
    std::map<wchar_t, int> advances;
    std::set<wchar_t> missing;

    explicit Font(const std::vector<std::string>& row) {
        dc = CreateCompatibleDC(nullptr);
        Require(dc != nullptr, "create memory device context");
        SetMapMode(dc, MM_TEXT);
        const auto faces = Split(row[FaceColumn], '|');
        for (std::size_t candidate = 0; candidate < faces.size(); ++candidate) {
            LOGFONTW query{};
            query.lfCharSet = DEFAULT_CHARSET;
            const auto name = Wide(faces[candidate]);
            wcsncpy_s(query.lfFaceName, name.c_str(), _TRUNCATE);
            bool available = false;
            EnumFontFamiliesExW(dc, &query, FoundFont, reinterpret_cast<LPARAM>(&available), 0);
            if (!available) {
                continue;
            }
            const auto number = [&](int column) {
                return std::stoi(Split(row[column], '|').at(candidate));
            };
            // Positive pixel cell height, charset and quality match CLinuxFont's Win32 branch.
            handle = CreateFontW(number(TallColumn), 0, 0, 0, number(WeightColumn), number(ItalicColumn),
                number(UnderlineColumn), number(StrikeoutColumn),
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                number(AntialiasColumn) ? ANTIALIASED_QUALITY : NONANTIALIASED_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE, name.c_str());
            Require(handle != nullptr, "create requested GDI font");
            original = SelectObject(dc, handle);
            TEXTMETRICW metrics{};
            Require(GetTextMetricsW(dc, &metrics) != 0, "measure GDI font metrics");
            height = metrics.tmHeight + number(ShadowColumn);
            wchar_t selected[TextFaceCapacity]{};
            Require(GetTextFaceW(dc, TextFaceCapacity, selected) != 0, "query selected font face");
            face = selected;
            return;
        }
        throw std::runtime_error("none of the scheme font candidates are installed: " + row[FaceColumn]);
    }

    ~Font() {
        if (original) {
            SelectObject(dc, original);
        }
        if (handle) {
            DeleteObject(handle);
        }
        if (dc) {
            DeleteDC(dc);
        }
    }

    int Advance(wchar_t character) {
        const auto found = advances.find(character);
        if (found != advances.end()) {
            return found->second;
        }
        if (character > L' ') {
            WORD glyph = 0;
            Require(GetGlyphIndicesW(dc, &character, 1, &glyph, GGI_MARK_NONEXISTING_GLYPHS) != GDI_ERROR,
                "query actual font glyph");
            if (glyph == MissingGlyph) {
                missing.insert(character);
            }
        }
        ABC abc{};
        Require(GetCharABCWidthsW(dc, character, character, &abc) != 0, "query Unicode ABC width");
        const int advance = abc.abcA + static_cast<int>(abc.abcB) + abc.abcC;
        advances[character] = advance;
        return advance;
    }
};

struct Size {
    int width = 0;
    int height = 0;
};

Size Measure(Font& font, const std::wstring& text, int drawWidth, bool wrap) {
    // Mirror TextImage::RecalculateNewLinePositions on the original string, including '&'.
    std::set<std::size_t> breaks;
    if (wrap) {
        std::size_t wordStart = 0;
        int lineWidth = 0;
        bool hasWord = false;
        bool justStartedNewLine = true;
        bool wordStartedOnNewLine = true;
        std::size_t index = !text.empty() && (text[0] == L'\r' || text[0] == L'\n') ? 1 : 0;
        for (; index < text.size(); ++index) {
            const wchar_t character = text[index];
            if (IsSpace(character)) {
                hasWord = false;
            } else if (!hasWord) {
                hasWord = true;
                wordStart = index;
                wordStartedOnNewLine = justStartedNewLine;
            }
            if (IsPrintable(character)) {
                lineWidth += font.Advance(character);
                justStartedNewLine = false;
            }
            if (lineWidth >= drawWidth || character == L'\r' || character == L'\n') {
                justStartedNewLine = true;
                hasWord = false;
                if (character != L'\r' && character != L'\n') {
                    if (wordStartedOnNewLine) {
                        breaks.insert(index);
                    } else {
                        breaks.insert(wordStart);
                        index = wordStart;
                    }
                }
                lineWidth = 0;
            }
        }
    }
    // Mirror TextImage::GetTextSize: &X displays X; && displays one ampersand.
    Size size{0, font.height};
    int lineWidth = 0;
    for (std::size_t index = 0; index < text.size(); ++index) {
        if (text[index] == L'&' && index + 1 < text.size()) {
            ++index;
        }
        const wchar_t character = text[index];
        lineWidth += font.Advance(character);
        if (character == L'\n' || (wrap && breaks.count(index) != 0)) {
            size.height += font.height;
            size.width = std::max(size.width, lineWidth);
            lineWidth = 0;
        }
    }
    size.width = std::max(size.width, lineWidth);
    return size;
}

std::string Flat(std::string text) {
    for (char& character : text) {
        if (character == '\t' || character == '\n' || character == '\r') {
            character = ' ';
        }
    }
    return text;
}
}

int wmain(int argc, wchar_t** argv) {
    try {
        Require(argc == 3, "usage: font_fit.exe input.tsv output.tsv");
        std::ifstream input(argv[1], std::ios::binary);
        std::ofstream output(argv[2], std::ios::binary);
        Require(input.good() && output.good(), "open measurement input and report");
        output << "language\tsource\tcontrol\ttoken\tfont\tfont_height\twidth\theight\tneeded_width"
            "\tneeded_height\tmissing_glyphs\tresult\ttext\n";
        std::string line;
        Require(static_cast<bool>(std::getline(input, line)), "read input header");
        std::map<std::string, std::unique_ptr<Font>> fonts;
        int controls = 0;
        int coverage = 0;
        int failures = 0;
        int negatives = 0;
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            auto row = Split(line, '\t');
            Require(row.size() == HeaderColumns, "unexpected input column count");
            for (auto& value : row) {
                value = Unescape(value);
            }
            std::string key;
            for (int column = FaceColumn; column < WidthColumn; ++column) {
                key += row[column] + '\t';
            }
            if (!fonts.count(key)) {
                fonts[key] = std::make_unique<Font>(row);
            }
            auto& font = *fonts[key];
            const auto text = Wide(row[TextColumn]);
            std::wstring missing;
            for (wchar_t character : text) {
                font.Advance(character);
                if (font.missing.count(character) != 0 && missing.find(character) == std::wstring::npos) {
                    missing += character;
                }
            }
            const int width = std::stoi(row[WidthColumn]);
            const int height = std::stoi(row[HeightColumn]);
            const int insetX = std::stoi(row[InsetXColumn]);
            const int insetY = std::stoi(row[InsetYColumn]);
            const int indicatorWidth = std::stoi(row[IndicatorWidthColumn]);
            const int indicatorHeight = std::stoi(row[IndicatorHeightColumn]);
            const auto size = Measure(font, text, std::max(1, width - insetX - indicatorWidth),
                row[WrapColumn] != "0");
            const int neededWidth = size.width + insetX + indicatorWidth;
            const int neededHeight = std::max(size.height, indicatorHeight) + insetY;
            const bool fits = neededWidth <= width && neededHeight <= height;
            std::string result;
            if (row[TypeColumn] == "coverage") {
                ++coverage;
                result = missing.empty() ? "glyphs-pass" : "missing-glyph";
                failures += !missing.empty();
            } else if (row[TypeColumn] == "negative") {
                Require(!fits, "negative oversized label was not detected");
                ++negatives;
                result = "expected-overflow";
            } else {
                ++controls;
                result = missing.empty() ? (fits ? "fit" : "overflow") : "missing-glyph";
                failures += !fits || !missing.empty();
            }
            output << row[LanguageColumn] << '\t' << row[SourceColumn] << '\t' << row[ControlColumn] << '\t'
                << row[TokenColumn] << '\t'
                << Utf8(font.face) << '\t' << font.height << '\t' << width << '\t' << height << '\t'
                << neededWidth << '\t' << neededHeight << '\t' << Utf8(missing) << '\t' << result << '\t'
                << Flat(row[TextColumn]) << '\n';
        }
        Require(controls > 0 && coverage == ExpectedAlphabetChecks && negatives == LanguageCount,
            "missing control, alphabet or negative cases");
        std::cout << "Win32 x86 GDI: " << controls << " controls, " << coverage << " alphabet checks, "
            << negatives << " detected negative controls; " << failures << " failures; " << fonts.size()
            << " actual font configurations\n";
        return failures == 0 ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 2;
    }
}
