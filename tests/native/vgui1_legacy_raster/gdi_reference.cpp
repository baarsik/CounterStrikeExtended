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

    const unsigned char* pixels() const {
        return _pixels;
    }
    int bufferWide() const {
        return _bufferWide;
    }
    int bufferTall() const {
        return _bufferTall;
    }

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


void ProbeOrder(const char* order, const std::vector<int>& bytes) {
    const Request request{"owned", "CSE Owned Raster V1", 20};
    RealGdiFont font(request);
    char selected[256]{};
    Require(GetTextFaceA(font.dc(), sizeof(selected), selected) > 0
        && std::strcmp(selected, request.name) == 0, "the actual selected family is authored input");
    const DWORD selectedBytes = GetFontData(font.dc(), 0, 0, nullptr, 0);
    Require(selectedBytes == 3240, "selected memory-font program has the authored byte count");
    std::vector<unsigned char> selectedProgram(selectedBytes);
    Require(GetFontData(font.dc(), 0, 0, selectedProgram.data(), selectedBytes) == selectedBytes,
        "exact selected memory-font program read");
    const std::string selectedHash = Hash(selectedProgram.data(), selectedProgram.size());
    Require(selectedHash == "069885a3e451df7439b6ae8279e942b22bf4017468e4868dd94ae936bdf2659a",
        "GDI selects the exact authored SFNT bytes, not another same-name face");
    const auto& metrics = font.metrics();
    const int canvasWide = metrics.tmMaxCharWidth + kPenX + kGuardCells;
    const int canvasTall = metrics.tmHeight + kPenY + kGuardCells;
    const std::size_t canvasBytes = static_cast<std::size_t>(canvasWide) * canvasTall * VGUI_RGBA_BYTES;
    const std::size_t sourceBytes = static_cast<std::size_t>(font.bufferWide()) * font.bufferTall() * VGUI_RGBA_BYTES;
    const std::string prefix(order);
    FILE* meta = Open((prefix + "-metrics.tsv").c_str());
    FILE* widths = Open((prefix + "-abc.tsv").c_str());
    FILE* glyphs = Open((prefix + "-glyphs.tsv").c_str());
    FILE* rgbaOutput = Open((prefix + "-rgba.bin").c_str());
    FILE* bgrOutput = Open((prefix + "-bgr.bin").c_str());
    std::fprintf(meta, "%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%u\t%d\t%d\t%d\t%d\t%zu\n",
        metrics.tmHeight, metrics.tmAscent, metrics.tmDescent, metrics.tmInternalLeading,
        metrics.tmExternalLeading, metrics.tmAveCharWidth, metrics.tmMaxCharWidth, metrics.tmWeight,
        static_cast<unsigned int>(metrics.tmCharSet), canvasWide, canvasTall,
        font.bufferWide(), font.bufferTall(), bytes.size());
    std::fprintf(meta, "selected-sfnt\t%lu\t%s\n", selectedBytes, selectedHash.c_str());
    for (const unsigned int byte : {0U, 9U, 32U, 65U, 66U, 128U}) {
        ABC measured{};
        SetLastError(0);
        const BOOL result = GetCharABCWidthsA(font.dc(), byte, byte, &measured);
        const DWORD error = GetLastError();
        std::fprintf(meta, "single-ABC\t%u\t%d\t%lu\t%d\t%u\t%d\n", byte, result != FALSE,
            error, measured.abcA, measured.abcB, measured.abcC);
    }
    ABC all[GLYPH_COUNT]{};
    SetLastError(0);
    const BOOL allResult = GetCharABCWidthsA(font.dc(), 0, GLYPH_COUNT - 1, all);
    std::fprintf(meta, "all-ABC\t%d\t%lu\tpitch-family\t%u\n", allResult != FALSE,
        GetLastError(), static_cast<unsigned int>(metrics.tmPitchAndFamily));
    Require(allResult != FALSE, "ordinary authored font supports the complete GDI ABC query");
    std::vector<unsigned char> rgba(canvasBytes);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        const int byte = bytes[index];
        int a = 0;
        int b = 0;
        int c = 0;
        font.getCharABCwide(byte, a, b, c);
        const ABC& raw = font.abc(byte);
        wchar_t unit = 0;
        const char character = static_cast<char>(byte);
        Require(MultiByteToWideChar(1252, 0, &character, 1, &unit, 1) == 1, "actual CP1252 conversion");
        WORD glyph = 0;
        Require(GetGlyphIndicesA(font.dc(), &character, 1, &glyph, GGI_MARK_NONEXISTING_GLYPHS) != GDI_ERROR,
            "actual authored ANSI glyph identity");
        std::fprintf(widths, "%zu\t%d\t%u\t%u\t%d\t%u\t%d\t%d\t%d\t%d\n", index, byte,
            static_cast<unsigned int>(unit), static_cast<unsigned int>(glyph), raw.abcA, raw.abcB, raw.abcC, a, b, c);
        std::fill(rgba.begin(), rgba.end(), static_cast<unsigned char>(0x5a));
        font.getCharRGBA(byte, kPenX, kPenY, canvasWide, canvasTall, rgba.data());
        Require(GdiFlush() != FALSE, "finish exact production raster call");
        Require(std::fwrite(rgba.data(), 1, rgba.size(), rgbaOutput) == rgba.size()
            && std::fwrite(font.pixels(), 1, sourceBytes, bgrOutput) == sourceBytes,
            "owned exact RGBA and post-flush DIB comparison bytes");
        std::fprintf(glyphs, "%zu\t%d\t%s\t%s\n", index, byte,
            Hash(rgba.data(), rgba.size()).c_str(), Hash(font.pixels(), sourceBytes).c_str());
    }
    for (FILE* file : {meta, widths, glyphs, rgbaOutput, bgrOutput}) {
        Require(std::fclose(file) == 0, "owned reference output close");
    }
}

void Smoothing(FILE* output, UINT action, const char* name) {
    UINT value = 0;
    const BOOL result = SystemParametersInfoW(action, 0, &value, 0);
    std::fprintf(output, "smoothing\t%s\t%d\t%u\t%lu\n", name, result != FALSE, value,
        result ? 0UL : GetLastError());
}
}

int wmain(int argc, wchar_t** argv) {
    Require(argc == 2, "pass only the authored font input");
    const auto owned = ReadFileBytes(argv[1]);
    Require(Hash(owned.data(), owned.size()) == "069885a3e451df7439b6ae8279e942b22bf4017468e4868dd94ae936bdf2659a",
        "exact authored font bytes");
    FILE* inputs = Open("inputs.tsv");
    std::fprintf(inputs, "compiler\t%u\tptr%zu\twchar%zu\tACP%u\tfont\t%s\n", _MSC_FULL_VER,
        sizeof(void*), sizeof(wchar_t), GetACP(), Hash(owned.data(), owned.size()).c_str());
    Provider(inputs, "CreateFontA", &CreateFontA);
    Provider(inputs, "ExtTextOutA", &ExtTextOutA);
    Smoothing(inputs, SPI_GETFONTSMOOTHING, "enabled");
    Smoothing(inputs, SPI_GETFONTSMOOTHINGTYPE, "type");
    Smoothing(inputs, SPI_GETFONTSMOOTHINGORIENTATION, "orientation");
    Smoothing(inputs, SPI_GETFONTSMOOTHINGCONTRAST, "contrast");
    Require(std::fclose(inputs) == 0, "input receipt close");
    DWORD count = 0;
    HANDLE registration = AddFontMemResourceEx(const_cast<unsigned char*>(owned.data()),
        static_cast<DWORD>(owned.size()), nullptr, &count);
    Require(registration != nullptr && count > 0, "process-only authored memory-font registration");
    std::vector<int> ascending;
    std::vector<int> descending;
    for (int byte = 0; byte < GLYPH_COUNT; ++byte) {
        ascending.push_back(byte);
        descending.push_back(GLYPH_COUNT - 1 - byte);
    }
    ProbeOrder("ascending", ascending);
    ProbeOrder("descending", descending);
    ProbeOrder("sequence", {'A', ' ', 'C', ' ', 'I', 'C', 'A', '\t', 'B', 'A', 'A', ' '});
    Require(RemoveFontMemResourceEx(registration) != FALSE, "release only owned font registration");
    std::puts("PASS exact production GDI bodies on authored geometry; ordinary request and call-order controls");
}
