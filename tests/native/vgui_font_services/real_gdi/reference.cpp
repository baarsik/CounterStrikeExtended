#include <Windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

using uchar = unsigned char;
#define VGUI_RGBA_BYTES 4
#define GLYPH_COUNT 256

namespace {
constexpr std::size_t kMaximumFontBytes = 16 * 1024 * 1024;
constexpr int kCanvasLimit = 2048;
constexpr int kPenX = 3;
constexpr int kPenY = 5;
constexpr int kGuardCells = 4;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s winerr=%lu\n", message, GetLastError());
        std::abort();
    }
}

std::string Hex(const unsigned char* bytes, std::size_t count) {
    constexpr char kDigits[] = "0123456789abcdef";
    std::string text;
    text.reserve(count * 2);
    for (std::size_t index = 0; index < count; ++index) {
        text += kDigits[bytes[index] >> 4];
        text += kDigits[bytes[index] & 15];
    }
    return text;
}

std::string Hash(const unsigned char* data, std::size_t count) {
    Require(count <= (std::numeric_limits<ULONG>::max)(), "bounded SHA256 input");
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    Require(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0,
        "SHA256 provider");
    ULONG objectBytes = 0;
    ULONG copied = 0;
    Require(BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectBytes),
        sizeof(objectBytes), &copied, 0) >= 0, "SHA256 object size");
    std::vector<unsigned char> object(objectBytes);
    BCRYPT_HASH_HANDLE hash = nullptr;
    Require(BCryptCreateHash(algorithm, &hash, object.data(), objectBytes, nullptr, 0, 0) >= 0, "SHA256 object");
    Require(BCryptHashData(hash, const_cast<PUCHAR>(data), static_cast<ULONG>(count), 0) >= 0, "SHA256 data");
    unsigned char digest[32]{};
    Require(BCryptFinishHash(hash, digest, sizeof(digest), 0) >= 0, "SHA256 finish");
    Require(BCryptDestroyHash(hash) >= 0 && BCryptCloseAlgorithmProvider(algorithm, 0) >= 0, "SHA256 cleanup");
    return Hex(digest, sizeof(digest));
}

std::vector<unsigned char> ReadFileBytes(const wchar_t* path) {
    FILE* file = nullptr;
    Require(_wfopen_s(&file, path, L"rb") == 0 && file != nullptr, "read-only pinned input open");
    Require(_fseeki64(file, 0, SEEK_END) == 0, "pinned input length seek");
    const auto length = _ftelli64(file);
    Require(length >= 0 && static_cast<std::uint64_t>(length) <= kMaximumFontBytes, "bounded pinned input length");
    Require(_fseeki64(file, 0, SEEK_SET) == 0, "pinned input rewind");
    std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
    Require(std::fread(bytes.data(), 1, bytes.size(), file) == bytes.size(), "pinned input exact read");
    Require(std::fclose(file) == 0, "pinned input close");
    return bytes;
}

std::string Utf8(const wchar_t* value) {
    const int count = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    Require(count > 0, "UTF8 path length");
    std::vector<char> text(static_cast<std::size_t>(count));
    Require(WideCharToMultiByte(CP_UTF8, 0, value, -1, text.data(), count, nullptr, nullptr) == count,
        "UTF8 path bytes");
    return text.data();
}

template <typename Function> void Provider(FILE* output, const char* symbol, Function function) {
    const void* address = nullptr;
    static_assert(sizeof(address) == sizeof(function), "Native Windows function address width");
    std::memcpy(&address, &function, sizeof(address));
    HMODULE module = nullptr;
    Require(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        static_cast<const wchar_t*>(address), &module) != FALSE, "actual GDI function owner");
    wchar_t path[32768]{};
    Require(GetModuleFileNameW(module, path, static_cast<DWORD>(sizeof(path) / sizeof(path[0]))) > 0,
        "actual GDI owner path");
    const auto data = ReadFileBytes(path);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(reinterpret_cast<const unsigned char*>(module)
        + dos->e_lfanew);
    std::fprintf(output, "provider\t%s\t%s\t%s\t%04x\n", symbol, Utf8(path).c_str(),
        Hash(data.data(), data.size()).c_str(), nt->FileHeader.Machine);
}

FILE* Open(const char* path) {
    FILE* file = nullptr;
    Require(fopen_s(&file, path, "wb") == 0 && file != nullptr, "owned receipt open");
    return file;
}

std::uint16_t Big16(const unsigned char* data) {
    return static_cast<std::uint16_t>((static_cast<unsigned int>(data[0]) << 8) | data[1]);
}

struct Request {
    const char* id;
    const char* name;
    int tall = 20;
    int wide = 0;
    float rotation = 0;
    int weight = 400;
    bool italic = false;
    bool underline = false;
    bool strikeout = false;
    bool symbol = false;
};

class RasterContract {
public:
    virtual ~RasterContract() = default;
    virtual void getCharRGBA(int, int, int, int, int, uchar*) = 0;
    virtual void getCharABCwide(int, int&, int&, int&) = 0;
    virtual int getTall() = 0;
};

class RealGdiFont final : public RasterContract {
public:
    explicit RealGdiFont(const Request& request) {
        const char* name = request.name;
        const int tall = request.tall;
        const int wide = request.wide;
        const float rotation = request.rotation;
        const int weight = request.weight;
        const bool italic = request.italic;
        const bool underline = request.underline;
        const bool strikeout = request.strikeout;
        const bool symbol = request.symbol;
#include "actual_constructor.inc"
        Require(_font && _dc && _bitmap && _pixels, "actual constructor acquired owned GDI resources");
        Require(_bufferWide > 0 && _bufferWide < kCanvasLimit && _metrics.tmHeight > 0
            && _metrics.tmHeight < kCanvasLimit, "bounded actual GDI geometry");
        Require(GdiFlush() != FALSE, "synchronize initial memory DIB");
    }

    ~RealGdiFont() override {
        Require(DeleteDC(_dc) != FALSE, "destroy owned DC before its selected objects");
        Require(DeleteObject(_font) != FALSE && DeleteObject(_bitmap) != FALSE, "destroy owned font/DIB");
    }

#include "actual_methods.inc"

    HDC dc() const {
        return _dc;
    }
    const TEXTMETRICA& metrics() const {
        return _metrics;
    }
    const ABC& abc(int ch) const {
        return _abc[ch];
    }

private:
    HFONT _font = nullptr;
    HDC _dc = nullptr;
    HBITMAP _bitmap = nullptr;
    TEXTMETRICA _metrics{};
    ABC _abc[GLYPH_COUNT]{};
    int _bufferWide = 0;
    int _bufferTall = 0;
    uchar* _pixels = nullptr;
};

void Names(FILE* output, const char* stage, const Request& request, HDC dc) {
    constexpr DWORD kNameTable = static_cast<DWORD>('n') | (static_cast<DWORD>('a') << 8)
        | (static_cast<DWORD>('m') << 16) | (static_cast<DWORD>('e') << 24);
    const DWORD count = GetFontData(dc, kNameTable, 0, nullptr, 0);
    if (count == GDI_ERROR) {
        std::fprintf(output, "%s\t%s\tno-sfnt-name-table\n", stage, request.id);
        return;
    }
    Require(count >= 6 && count <= kMaximumFontBytes, "bounded selected font name table");
    std::vector<unsigned char> data(count);
    Require(GetFontData(dc, kNameTable, 0, data.data(), count) == count, "selected font exact name table");
    const std::size_t names = Big16(data.data() + 2);
    const std::size_t stringOffset = Big16(data.data() + 4);
    constexpr std::size_t kRecordBytes = 12;
    Require(names <= (data.size() - 6) / kRecordBytes && stringOffset <= data.size(), "name records bounded");
    for (std::size_t index = 0; index < names; ++index) {
        const auto* record = data.data() + 6 + index * kRecordBytes;
        const auto platform = Big16(record);
        const auto language = Big16(record + 4);
        const auto name = Big16(record + 6);
        const std::size_t length = Big16(record + 8);
        const std::size_t offset = Big16(record + 10);
        if (platform != 3 || (name != 1 && name != 2 && name != 4 && name != 6 && name != 16 && name != 17)) {
            continue;
        }
        Require(offset <= data.size() - stringOffset && length <= data.size() - stringOffset - offset
            && (length & 1) == 0, "UTF16 font name field bounds");
        std::fprintf(output, "%s\t%s\t%u\t%u\t%s\n", stage, request.id, static_cast<unsigned int>(name), static_cast<unsigned int>(language),
            Hex(data.data() + stringOffset + offset, length).c_str());
    }
}

void Probe(FILE* metrics, FILE* widths, FILE* glyphs, FILE* names, const char* stage, const Request& request) {
    RealGdiFont font(request);
    char selected[256]{};
    Require(GetTextFaceA(font.dc(), static_cast<int>(sizeof(selected)), selected) > 0, "actual selected face name");
    const auto& tm = font.metrics();
    const DWORD dataBytes = GetFontData(font.dc(), 0, 0, nullptr, 0);
    std::string program = "no-sfnt-program";
    if (dataBytes != GDI_ERROR) {
        Require(dataBytes > 0 && dataBytes <= kMaximumFontBytes, "bounded selected font program");
        std::vector<unsigned char> data(dataBytes);
        Require(GetFontData(font.dc(), 0, 0, data.data(), dataBytes) == dataBytes, "selected font exact program");
        program = Hash(data.data(), data.size());
    }
    std::fprintf(metrics, "%s\t%s\t%s\t%d\t%d\t%.3f\t%d\t%d%d%d%d\t%s\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%u\t%u\t%u\t%u\t%d\t%d\t%s\n",
        stage, request.id, request.name, request.tall, request.wide, static_cast<double>(request.rotation),
        request.weight, request.italic, request.underline, request.strikeout, request.symbol, selected,
        tm.tmHeight, tm.tmAscent, tm.tmDescent, tm.tmInternalLeading, tm.tmExternalLeading, tm.tmAveCharWidth,
        tm.tmMaxCharWidth, tm.tmWeight, static_cast<unsigned int>(tm.tmItalic),
        static_cast<unsigned int>(tm.tmUnderlined), static_cast<unsigned int>(tm.tmStruckOut),
        static_cast<unsigned int>(tm.tmCharSet),
        GetDeviceCaps(font.dc(), LOGPIXELSX), GetDeviceCaps(font.dc(), LOGPIXELSY), program.c_str());
    Names(names, stage, request, font.dc());
    const int canvasWide = tm.tmMaxCharWidth + kPenX + kGuardCells;
    const int canvasTall = tm.tmHeight + kPenY + kGuardCells;
    std::vector<unsigned char> rgba(static_cast<std::size_t>(canvasWide) * static_cast<std::size_t>(canvasTall)
        * VGUI_RGBA_BYTES);
    for (int ch = 0; ch < GLYPH_COUNT; ++ch) {
        int a = 0;
        int b = 0;
        int c = 0;
        font.getCharABCwide(ch, a, b, c);
        const ABC& raw = font.abc(ch);
        std::fprintf(widths, "%s\t%s\t%d\t%d\t%u\t%d\t%d\t%d\t%d\n",
            stage, request.id, ch, raw.abcA, raw.abcB, raw.abcC, a, b, c);
        std::fill(rgba.begin(), rgba.end(), static_cast<unsigned char>(0));
        font.getCharRGBA(ch, kPenX, kPenY, canvasWide, canvasTall, rgba.data());
        Require(GdiFlush() != FALSE, "finish actual memory glyph drawing");
        std::size_t coveragePixels = 0;
        std::uint64_t coverageSum = 0;
        for (std::size_t byte = 3; byte < rgba.size(); byte += VGUI_RGBA_BYTES) {
            coveragePixels += rgba[byte] != 0;
            coverageSum += rgba[byte];
        }
        std::fprintf(glyphs, "%s\t%s\t%d\t%d\t%d\t%zu\t%llu\t%s\n", stage, request.id, ch,
            canvasWide, canvasTall, coveragePixels, static_cast<unsigned long long>(coverageSum),
            Hash(rgba.data(), rgba.size()).c_str());
        if (ch == 'A' || ch == 'a') {
            const std::string filename = std::string(stage) + "-" + request.id + "-" + std::to_string(ch) + ".rgba";
            FILE* image = Open(filename.c_str());
            Require(std::fwrite(rgba.data(), 1, rgba.size(), image) == rgba.size() && std::fclose(image) == 0,
                "owned raster reference bytes");
        }
    }
    SIZE pair{};
    Require(GetTextExtentPoint32A(font.dc(), "AV", 2, &pair) != FALSE, "actual whole-string pair extent");
    Require(MoveToEx(font.dc(), 0, 0, nullptr) != FALSE, "pair origin");
    Require(ExtTextOutA(font.dc(), 0, 0, 0, nullptr, "AV", 2, nullptr) != FALSE, "actual standalone pair draw");
    POINT advance{};
    Require(GetCurrentPositionEx(font.dc(), &advance) != FALSE, "actual whole-string current-position advance");
    const DWORD kerningPairs = GetKerningPairsA(font.dc(), 0, nullptr);
    std::fprintf(metrics, "%s\t%s\tpair-AV\t%ld\t%ld\t%ld\t%ld\t%lu\n", stage, request.id,
        pair.cx, pair.cy, advance.x, advance.y, kerningPairs);
}
}

int wmain(int argc, wchar_t** argv) {
    Require(argc == 2, "pass the pinned game Marlett input path");
    FILE* metrics = Open("metrics.tsv");
    FILE* widths = Open("abc.tsv");
    FILE* glyphs = Open("glyphs.tsv");
    FILE* names = Open("names.tsv");
    FILE* inputs = Open("inputs.tsv");
    std::fprintf(inputs, "compiler\t%u\tptr%zu\twchar%zu\tACP%u\tOEM%u\n", _MSC_FULL_VER,
        sizeof(void*), sizeof(wchar_t), GetACP(), GetOEMCP());
    Provider(inputs, "CreateFontA", &CreateFontA);
    Provider(inputs, "GetFontData", &GetFontData);
    Provider(inputs, "ExtTextOutA", &ExtTextOutA);
    std::fprintf(inputs, "batch-limit\t%lu\n", GdiGetBatchLimit());
    const auto game = ReadFileBytes(argv[1]);
    const auto gameHash = Hash(game.data(), game.size());
    Require(gameHash == "3b6192eca4451835f4d634b0b6b0841da321ec65c1f3f1aa22d186da45787b9c",
        "game Marlett bytes match frozen V1 input");
    std::fprintf(inputs, "game-marlett\t%zu\t%s\n", game.size(), gameHash.c_str());
    std::vector<Request> requests{{"regular", "Arial"}, {"negative-height", "Arial", -20},
        {"default-height", "Arial", 0}, {"width10", "Arial", 20, 10},
        {"width-minus10", "Arial", 20, -10}, {"angle90", "Arial", 20, 0, 90.0f},
        {"angle-minus90", "Arial", 20, 0, -90.0f}, {"angle-fraction", "Arial", 20, 0, 12.375f},
        {"angle-minus-fraction", "Arial", 20, 0, -12.375f}, {"weight0", "Arial", 20, 0, 0, 0},
        {"weight200", "Arial", 20, 0, 0, 200}, {"weight600", "Arial", 20, 0, 0, 600},
        {"weight700", "Arial", 20, 0, 0, 700}, {"weight800", "Arial", 20, 0, 0, 800},
        {"weight1000", "Arial", 20, 0, 0, 1000}, {"italic", "Arial", 20, 0, 0, 400, true},
        {"underline", "Arial", 20, 0, 0, 400, false, true},
        {"strikeout", "Arial", 20, 0, 0, 400, false, false, true},
        {"all-styles", "Arial", 20, 0, 0, 700, true, true, true},
        {"symbol-ascii-family", "Arial", 20, 0, 0, 400, false, false, false, true},
        {"narrow-bold-name", "Arial Narrow Bold", 20}, {"verdana-bold-name", "Verdana Bold", 20, 0, 0, 500},
        {"tahoma", "Tahoma", 16, 0, 0, 500}, {"fixedsys", "FixedSys", 18},
        {"marlett", "Marlett", 16, 0, 0, 0, false, false, false, true}};
    for (const auto& request : requests) {
        Probe(metrics, widths, glyphs, names, "system", request);
    }
    Require(AddFontResourceExW(argv[1], FR_PRIVATE, nullptr) > 0, "own process-only game font registration");
    Probe(metrics, widths, glyphs, names, "private-game-font", requests.back());
    Require(RemoveFontResourceExW(argv[1], FR_PRIVATE, nullptr) != FALSE, "release owned game font registration");
    Probe(metrics, widths, glyphs, names, "after-private-release", requests.back());
    const auto after = ReadFileBytes(argv[1]);
    Require(Hash(after.data(), after.size()) == gameHash, "game input bytes unchanged");
    for (FILE* output : {metrics, widths, glyphs, names, inputs}) {
        Require(std::fclose(output) == 0, "owned reference output close");
    }
    std::puts("PASS: actual GDI memory reference; exact production constructor/raster/ABC bodies; owned cleanup");
    return 0;
}
