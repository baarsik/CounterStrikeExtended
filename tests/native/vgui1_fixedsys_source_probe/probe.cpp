#include <Windows.h>
#include <dwrite.h>
#include <cwchar>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_WINFONTS_H
#include FT_MODULE_H
#include "VGui1LegacyRaster.h"
#include "gdi_owner.inc"

namespace {
constexpr std::size_t MaximumAssetName = 2048;
constexpr int CanvasWidth = 24;
constexpr int CanvasHeight = 24;
constexpr int FixedCellWidth = 8;
constexpr int FixedCellHeight = 15;
constexpr std::size_t CanvasBytes = CanvasWidth * CanvasHeight * VGUI_RGBA_BYTES;

void DirectWriteSource(FILE* output, HDC dc) {
    IDWriteFactory* factory = nullptr;
    HRESULT result = DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&factory));
    std::fprintf(output, "dwrite-factory\t%08lx\n", static_cast<unsigned long>(result));
    if (FAILED(result)) {
        return;
    }
    IDWriteGdiInterop* interop = nullptr;
    result = factory->GetGdiInterop(&interop);
    std::fprintf(output, "dwrite-interop\t%08lx\n", static_cast<unsigned long>(result));
    if (SUCCEEDED(result)) {
        IDWriteFontFace* face = nullptr;
        result = interop->CreateFontFaceFromHdc(dc, &face);
        std::fprintf(output, "dwrite-selected-hdc\t%08lx\n", static_cast<unsigned long>(result));
        if (SUCCEEDED(result)) {
            UINT32 count = 0;
            result = face->GetFiles(&count, nullptr);
            std::fprintf(output, "dwrite-face\ttype=%u\tindex=%u\tfiles=%u\tresult=%08lx\n",
                static_cast<unsigned int>(face->GetType()), face->GetIndex(), count, static_cast<unsigned long>(result));
            Require(SUCCEEDED(result) && count <= 8, "bounded selected HDC font files");
            std::vector<IDWriteFontFile*> files(count, nullptr);
            result = face->GetFiles(&count, files.data());
            Require(SUCCEEDED(result), "selected font file references");
            for (auto* file : files) {
                const void* key = nullptr;
                UINT32 keyBytes = 0;
                IDWriteFontFileLoader* loader = nullptr;
                IDWriteLocalFontFileLoader* local = nullptr;
                result = file->GetReferenceKey(&key, &keyBytes);
                Require(SUCCEEDED(result) && SUCCEEDED(file->GetLoader(&loader)), "selected file key/loader");
                result = loader->QueryInterface(__uuidof(IDWriteLocalFontFileLoader), reinterpret_cast<void**>(&local));
                if (SUCCEEDED(result)) {
                    UINT32 length = 0;
                    result = local->GetFilePathLengthFromKey(key, keyBytes, &length);
                    Require(SUCCEEDED(result) && length < MaximumAssetName, "bounded selected file path");
                    std::vector<wchar_t> path(static_cast<std::size_t>(length) + 1);
                    Require(SUCCEEDED(local->GetFilePathFromKey(key, keyBytes, path.data(), length + 1)),
                        "selected HDC exact local font path");
                    std::fprintf(output, "dwrite-selected-file\t%s\n", Utf8(path.data()).c_str());
                    local->Release();
                } else {
                    std::fprintf(output, "dwrite-nonlocal-file\t%08lx\n", static_cast<unsigned long>(result));
                }
                loader->Release();
                file->Release();
            }
            face->Release();
        }
        interop->Release();
    }
    factory->Release();
}

std::wstring FixedSysRegistryCandidate(FILE* output) {
    constexpr wchar_t Key[] = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts";
    constexpr wchar_t Value[] = L"Fixedsys (All res)";
    wchar_t name[MaximumAssetName]{};
    DWORD count = sizeof(name);
    DWORD type = 0;
    const LONG status = RegGetValueW(HKEY_LOCAL_MACHINE, Key, Value, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ,
        &type, name, &count);
    std::fprintf(output, "exact-registry-candidate\tstatus=%ld\ttype=%lu\tbytes=%lu\tname=%s\n",
        status, type, count, status == ERROR_SUCCESS ? Utf8(name).c_str() : "unavailable");
    if (status != ERROR_SUCCESS) {
        return {};
    }
    Require(count >= sizeof(wchar_t) && count <= sizeof(name) && name[0], "bounded exact registry value");
    if (std::wcschr(name, L'\\') || std::wcschr(name, L'/')) {
        return name;
    }
    wchar_t windows[MaximumAssetName]{};
    const UINT length = GetWindowsDirectoryW(windows, static_cast<UINT>(MaximumAssetName));
    Require(length > 0 && length < MaximumAssetName, "Windows system directory");
    return std::wstring(windows) + L"\\Fonts\\" + name;
}

std::uint16_t Little16(const std::vector<unsigned char>& bytes, std::size_t offset) {
    Require(offset <= bytes.size() && bytes.size() - offset >= 2, "bounded NE word");
    return static_cast<std::uint16_t>(bytes[offset] | (static_cast<unsigned int>(bytes[offset + 1]) << 8));
}

std::uint32_t Little32(const std::vector<unsigned char>& bytes, std::size_t offset) {
    return Little16(bytes, offset) | (static_cast<std::uint32_t>(Little16(bytes, offset + 2)) << 16);
}

void CandidateResources(FILE* metadata, const std::vector<unsigned char>& bytes) {
    constexpr std::uint16_t FontResourceType = 0x8008;
    constexpr std::size_t NeResourceRecordBytes = 12;
    Require(Little16(bytes, 0) == 0x5a4d, "candidate MZ header");
    const std::size_t ne = Little32(bytes, 0x3c);
    Require(Little16(bytes, ne) == 0x454e, "candidate NE resource container");
    const std::size_t table = ne + Little16(bytes, ne + 0x24);
    const std::size_t end = ne + Little16(bytes, ne + 0x26);
    const unsigned int shift = Little16(bytes, table);
    Require(table < end && end <= bytes.size() && shift <= 16, "bounded NE resource table/alignment");
    std::size_t offset = table + 2;
    for (unsigned int group = 0; group < 16; ++group) {
        const std::uint16_t type = Little16(bytes, offset);
        if (type == 0) {
            return;
        }
        const std::size_t count = Little16(bytes, offset + 2);
        offset += 8;
        Require(count <= 16 && offset <= end && count <= (end - offset) / NeResourceRecordBytes,
            "bounded candidate resource records");
        for (std::size_t index = 0; index < count; ++index) {
            const std::size_t data = static_cast<std::size_t>(Little16(bytes, offset)) << shift;
            const std::size_t length = static_cast<std::size_t>(Little16(bytes, offset + 2)) << shift;
            Require(data <= bytes.size() && length <= bytes.size() - data, "bounded NE resource data");
            if (type == FontResourceType) {
                const std::size_t declared = Little32(bytes, data + 2);
                Require(declared > 0 && declared <= length, "FNT header length within NE font resource");
                std::fprintf(metadata, "ne-font-resource\tid=%u\toffset=%zu\tpadded-bytes=%zu"
                    "\tfnt-bytes=%zu\tsha256=%s\n", Little16(bytes, offset + 6), data, length, declared,
                    Hash(bytes.data() + data, declared).c_str());
            }
            offset += NeResourceRecordBytes;
        }
    }
    Require(false, "candidate resource table terminates within finite bound");
}

void GdiCellFacts(FILE* output, unsigned int byte, const std::array<unsigned char, CanvasBytes>& rgba,
    int a, int b, int c) {
    unsigned int count = 0;
    unsigned int alphaSum = 0;
    int minimumX = CanvasWidth;
    int minimumY = CanvasHeight;
    int maximumX = 0;
    int maximumY = 0;
    std::array<bool, 256> alphaValues{};
    for (int y = 0; y < FixedCellHeight; ++y) {
        for (int x = 0; x < FixedCellWidth; ++x) {
            const auto alpha = rgba[(static_cast<std::size_t>(y + kPenY) * CanvasWidth + x + kPenX)
                * VGUI_RGBA_BYTES + 3];
            alphaValues[alpha] = true;
            alphaSum += alpha;
            if (alpha != 0) {
                ++count;
                minimumX = (std::min)(minimumX, x);
                minimumY = (std::min)(minimumY, y);
                maximumX = (std::max)(maximumX, x + 1);
                maximumY = (std::max)(maximumY, y + 1);
            }
        }
    }
    std::fprintf(output, "gdi-facts\t%u\tABC=%d/%d/%d\tnonzero=%u\talpha-sum=%u\tbbox=%d/%d/%d/%d\talpha=",
        byte, a, b, c, count, alphaSum, minimumX, minimumY, maximumX, maximumY);
    for (std::size_t value = 0; value < alphaValues.size(); ++value) {
        if (alphaValues[value]) {
            std::fprintf(output, "%zu,", value);
        }
    }
    std::fputc('\n', output);
}

std::string FontCell(FT_Face face, unsigned long character, const TEXTMETRICA& metrics,
    unsigned int byte, FILE* output, const char* mode) {
    const FT_UInt glyph = FT_Get_Char_Index(face, character);
    const FT_Error error = FT_Load_Glyph(face, glyph, FT_LOAD_DEFAULT);
    if (error != 0) {
        std::fprintf(output, "ft-load-error\t%s\t%u\t%u\t%d\n", mode, byte, glyph, error);
        return "load-error";
    }
    const auto& bitmap = face->glyph->bitmap;
    Require(face->glyph->format == FT_GLYPH_FORMAT_BITMAP && bitmap.pixel_mode == FT_PIXEL_MODE_MONO
        && bitmap.width <= 256 && bitmap.rows <= 256 && bitmap.pitch >= 0, "actual WinFNT mono bitmap bounds");
    std::vector<unsigned char> bgr(static_cast<std::size_t>(metrics.tmMaxCharWidth)
        * static_cast<std::size_t>(metrics.tmHeight) * VGUI_RGBA_BYTES, 0);
    const int x = face->glyph->bitmap_left;
    const int y = metrics.tmAscent - face->glyph->bitmap_top;
    bool clipped = false;
    unsigned int ink = 0;
    for (unsigned int row = 0; row < bitmap.rows; ++row) {
        for (unsigned int column = 0; column < bitmap.width; ++column) {
            if ((bitmap.buffer[static_cast<std::size_t>(row) * static_cast<std::size_t>(bitmap.pitch) + column / 8]
                & (0x80U >> (column % 8))) == 0) {
                continue;
            }
            ++ink;
            const int outX = x + static_cast<int>(column);
            const int outY = y + static_cast<int>(row);
            if (outX < 0 || outY < 0 || outX >= metrics.tmMaxCharWidth || outY >= metrics.tmHeight) {
                clipped = true;
                continue;
            }
            auto* pixel = bgr.data() + (static_cast<std::size_t>(outY) * static_cast<std::size_t>(metrics.tmMaxCharWidth)
                + static_cast<std::size_t>(outX)) * VGUI_RGBA_BYTES;
            pixel[0] = pixel[1] = pixel[2] = 255;
        }
    }
    std::array<unsigned char, CanvasBytes> rgba{};
    rgba.fill(0x5a);
    const cse_vgui1::legacy::BgrView source{bgr.data(), bgr.size(), metrics.tmMaxCharWidth, metrics.tmHeight,
        static_cast<std::ptrdiff_t>(metrics.tmMaxCharWidth) * VGUI_RGBA_BYTES};
    const cse_vgui1::legacy::RgbaCanvas canvas{rgba.data(), rgba.size(), CanvasWidth, CanvasHeight};
    Require(cse_vgui1::legacy::TransferLegacyBgr(source, metrics.tmMaxCharWidth, metrics.tmHeight,
        kPenX, kPenY, canvas, byte == '\t') == cse_vgui1::legacy::Result::Ok, "unchanged legacy transfer kernel");
    const std::string digest = Hash(rgba.data(), rgba.size());
    std::fprintf(output, "ft-cell\t%s\t%u\t%lu\t%u\t%ld\t%d\t%d\t%u\t%u\t%d\t%u\t%s\n",
        mode, byte, character, glyph, face->glyph->advance.x, x, y, bitmap.width, bitmap.rows, clipped, ink, digest.c_str());
    return digest;
}

std::wstring ExactSystemCandidate(FILE* output) {
    // One explicitly named bitmap-font candidate, not an installed-font inventory.
    wchar_t windows[MaximumAssetName]{};
    const UINT length = GetWindowsDirectoryW(windows, static_cast<UINT>(MaximumAssetName));
    Require(length > 0 && length < MaximumAssetName, "Windows system directory");
    const std::wstring path = std::wstring(windows) + L"\\Fonts\\vgafix.fon";
    const DWORD attributes = GetFileAttributesW(path.c_str());
    std::fprintf(output, "explicit-system-candidate\t%s\tattributes=%lu\tdirect-selected-link=unproven\n",
        Utf8(path.c_str()).c_str(), attributes);
    return attributes == INVALID_FILE_ATTRIBUTES ? std::wstring{} : path;
}

void CompareCandidate(FILE* metadata, FILE* cells, RealGdiFont& font, const std::wstring& path) {
    const auto bytes = ReadFileBytes(path.c_str());
    std::fprintf(metadata, "candidate-asset\t%s\tbytes=%zu\tsha256=%s\n", Utf8(path.c_str()).c_str(),
        bytes.size(), Hash(bytes.data(), bytes.size()).c_str());
    CandidateResources(metadata, bytes);
    FT_Library library = nullptr;
    Require(FT_Init_FreeType(&library) == 0, "existing pinned FreeType context");
    int major = 0;
    int minor = 0;
    int patch = 0;
    FT_Library_Version(library, &major, &minor, &patch);
    Require(major == 2 && minor == 14 && patch == 3, "exact pinned FreeType2.14.3");
    std::fprintf(metadata, "freetype\t%d.%d.%d\twinfonts-module=%d\n", major, minor, patch,
        FT_Get_Module(library, "winfonts") != nullptr);
    FT_Face query = nullptr;
    const FT_Error queried = FT_New_Memory_Face(library, bytes.data(), static_cast<FT_Long>(bytes.size()), -1, &query);
    std::fprintf(metadata, "ft-file-query\terror=%d\tfaces=%ld\n", queried, query ? query->num_faces : 0);
    if (queried != 0) {
        FT_Done_FreeType(library);
        return;
    }
    const FT_Long faceCount = query->num_faces;
    Require(faceCount > 0 && faceCount <= 16, "finite candidate file faces only");
    FT_Done_Face(query);
    for (FT_Long index = 0; index < faceCount; ++index) {
        FT_Face face = nullptr;
        Require(FT_New_Memory_Face(library, bytes.data(), static_cast<FT_Long>(bytes.size()), index, &face) == 0,
            "candidate face transient bytes stay alive");
        FT_WinFNT_HeaderRec header{};
        const FT_Error result = FT_Get_WinFNT_Header(face, &header);
        std::fprintf(metadata, "ft-face\t%ld\tfamily=%s\tstyle=%s\tfixed-sizes=%d\tcharmaps=%d\tflags=%ld\tfnt=%d\n",
            index, face->family_name ? face->family_name : "", face->style_name ? face->style_name : "",
            face->num_fixed_sizes, face->num_charmaps, face->face_flags, result);
        if (result == 0) {
            char copyright[61]{};
            std::memcpy(copyright, header.copyright, sizeof(header.copyright));
            std::fprintf(metadata, "fnt-header\t%ld\tversion=%u\tsize=%lu\ttype=%u\tpoints=%u\tdpi=%u/%u"
                "\tascent=%u\tinternal=%u\texternal=%u\tweight=%u\tcharset=%u\tcell=%u/%u\tavg-max=%u/%u"
                "\tfirst-last-default-break=%u/%u/%u/%u\tstyles=%u/%u/%u\tcopyright=%s\n", index,
                header.version, header.file_size, header.file_type, header.nominal_point_size,
                header.horizontal_resolution, header.vertical_resolution, header.ascent, header.internal_leading,
                header.external_leading, header.weight, header.charset, header.pixel_width, header.pixel_height,
                header.avg_width, header.max_width, header.first_char, header.last_char, header.default_char,
                header.break_char, header.italic, header.underline, header.strike_out, copyright);
            if (face->family_name && _stricmp(face->family_name, "Fixedsys") == 0
                && face->num_fixed_sizes == 1 && header.pixel_height == font.metrics().tmHeight
                && header.pixel_width == font.metrics().tmMaxCharWidth && header.weight == 400
                && header.charset == ANSI_CHARSET && !header.italic && !header.underline && !header.strike_out) {
                Require(FT_Select_Size(face, 0) == 0, "candidate original strike, no scaling/filter tuning");
                std::fprintf(metadata, "ft-byte-charmap\tbefore-active=%d\tcount=%d\n",
                    face->charmap != nullptr, face->num_charmaps);
                Require(face->num_charmaps == 1, "only the selected ANSI WinFNT byte map");
                std::fprintf(metadata, "ft-byte-charmap\tencoding=%lu\tplatform=%u\tencoding-id=%u\n",
                    static_cast<unsigned long>(face->charmaps[0]->encoding), face->charmaps[0]->platform_id,
                    face->charmaps[0]->encoding_id);
                Require(FT_Set_Charmap(face, face->charmaps[0]) == 0,
                    "explicit non-Unicode WinFNT byte charmap, no inferred Unicode map");
                std::fprintf(metadata, "matching-strike\tface=%ld\tx-ppem=%u\ty-ppem=%u\tascent=%ld\tdescent=%ld"
                    "\theight=%ld\tmax-advance=%ld\n", index, face->size->metrics.x_ppem, face->size->metrics.y_ppem,
                    face->size->metrics.ascender, face->size->metrics.descender, face->size->metrics.height,
                    face->size->metrics.max_advance);
                std::array<unsigned char, CanvasBytes> gdi{};
                for (unsigned int byte = 0; byte < GLYPH_COUNT; ++byte) {
                    const char input = static_cast<char>(byte);
                    wchar_t unicode = 0;
                    Require(MultiByteToWideChar(1252, 0, &input, 1, &unicode, 1) == 1, "fixed CP1252 input boundary");
                    gdi.fill(0x5a);
                    font.getCharRGBA(static_cast<int>(byte), kPenX, kPenY, CanvasWidth, CanvasHeight, gdi.data());
                    Require(GdiFlush() != FALSE, "offscreen selected GDI glyph complete");
                    int a = 0;
                    int b = 0;
                    int c = 0;
                    font.getCharABCwide(static_cast<int>(byte), a, b, c);
                    GdiCellFacts(cells, byte, gdi, a, b, c);
                    const std::string digest = Hash(gdi.data(), gdi.size());
                    const auto raw = FontCell(face, byte, font.metrics(), byte, cells, "raw-byte");
                    const auto wide = FontCell(face, unicode, font.metrics(), byte, cells, "cp1252-unicode");
                    std::fprintf(cells, "compare\t%u\t%u\t%s\traw-equal=%d\tunicode-equal=%d\n", byte,
                        static_cast<unsigned int>(unicode), digest.c_str(), raw == digest, wide == digest);
                }
            }
        }
        FT_Done_Face(face);
    }
    Require(FT_Done_FreeType(library) == 0, "FreeType face/context released before input bytes die");
    Require(bytes == ReadFileBytes(path.c_str()), "candidate system file bytes unchanged after read-only comparison");
}
}

int main() {
    static_cast<void>(&Big16);
    FILE* metadata = Open("metadata.tsv");
    FILE* cells = Open("cells.tsv");
    FILE* providers = Open("providers.tsv");
    Provider(providers, "CreateFontA", &CreateFontA);
    Provider(providers, "ExtTextOutA", &ExtTextOutA);
    Provider(providers, "GetFontData", &GetFontData);
    Provider(providers, "DWriteCreateFactory", &DWriteCreateFactory);
    const DWORD before = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    {
        const Request request{"fixedsys-exact", "FixedSys", 18, 0, 0.0f, 400, false, false, false, false};
        RealGdiFont font(request);
        const auto& m = font.metrics();
        wchar_t name[256]{};
        Require(GetTextFaceW(font.dc(), 256, name) > 0, "actual selected GDI name");
        LOGFONTW logical{};
        Require(GetObjectW(GetCurrentObject(font.dc(), OBJ_FONT), static_cast<int>(sizeof(logical)), &logical)
            == static_cast<int>(sizeof(logical)),
            "actual selected HFONT logical object");
        std::fprintf(metadata, "exact-request\tFixedSys\t18/0/0/400\tANSI/OUT_DEFAULT/CLIP_DEFAULT/DEFAULT_QUALITY/DEFAULT_PITCH\n");
        std::fprintf(metadata, "gdi-selected\t%s\tACP=%u\tH/A/D/IL/EL=%ld/%ld/%ld/%ld/%ld\tavg/max=%ld/%ld"
            "\tweight=%ld\tpitch-family=%u\tcharset=%u\tfirst/last/default=%u/%u/%u\tdpi=%d/%d\n",
            Utf8(name).c_str(), GetACP(), m.tmHeight, m.tmAscent, m.tmDescent, m.tmInternalLeading, m.tmExternalLeading,
            m.tmAveCharWidth, m.tmMaxCharWidth, m.tmWeight, m.tmPitchAndFamily, m.tmCharSet,
            static_cast<unsigned char>(m.tmFirstChar), static_cast<unsigned char>(m.tmLastChar),
            static_cast<unsigned char>(m.tmDefaultChar), GetDeviceCaps(font.dc(), LOGPIXELSX), GetDeviceCaps(font.dc(), LOGPIXELSY));
        std::fprintf(metadata, "logical-hfont\t%s\theight=%ld\twidth=%ld\tweight=%ld\tcharset=%u\tquality=%u"
            "\toutput=%u\tclip=%u\tpitch-family=%u\n", Utf8(logical.lfFaceName).c_str(), logical.lfHeight,
            logical.lfWidth, logical.lfWeight, logical.lfCharSet, logical.lfQuality, logical.lfOutPrecision,
            logical.lfClipPrecision, logical.lfPitchAndFamily);
        SetLastError(0);
        const DWORD sfnt = GetFontData(font.dc(), 0, 0, nullptr, 0);
        std::fprintf(metadata, "selected-sfnt\tbytes=%lu\terror=%lu\n", sfnt, GetLastError());
        SetLastError(0);
        const UINT outline = GetOutlineTextMetricsW(font.dc(), 0, nullptr);
        std::fprintf(metadata, "selected-outline\tbytes=%u\terror=%lu\n", outline, GetLastError());
        BOOL smoothing = FALSE;
        UINT smoothingType = 0;
        UINT smoothingContrast = 0;
        UINT smoothingOrientation = 0;
        Require(SystemParametersInfoW(SPI_GETFONTSMOOTHING, 0, &smoothing, 0) != FALSE,
            "read existing smoothing state only");
        Require(SystemParametersInfoW(SPI_GETFONTSMOOTHINGTYPE, 0, &smoothingType, 0) != FALSE
            && SystemParametersInfoW(SPI_GETFONTSMOOTHINGCONTRAST, 0, &smoothingContrast, 0) != FALSE
            && SystemParametersInfoW(SPI_GETFONTSMOOTHINGORIENTATION, 0, &smoothingOrientation, 0) != FALSE,
            "read existing smoothing parameters only");
        std::fprintf(metadata, "smoothing-read-only\tenabled=%d\ttype=%u\tcontrast=%u\torientation=%u\n",
            smoothing, smoothingType, smoothingContrast, smoothingOrientation);
        const HGDIOBJ selected = GetCurrentObject(font.dc(), OBJ_FONT);
        DirectWriteSource(metadata, font.dc());
        Require(selected == GetCurrentObject(font.dc(), OBJ_FONT), "selected font unchanged by metadata query");
        auto candidate = FixedSysRegistryCandidate(metadata);
        if (candidate.empty()) {
            candidate = ExactSystemCandidate(metadata);
        }
        if (!candidate.empty()) {
            Require(GetACP() == 1252, "reference byte domain remains ACP1252, no locale mutation");
            CompareCandidate(metadata, cells, font, candidate);
        }
    }
    std::fprintf(metadata, "gdi-objects\tbefore=%lu\tafter=%lu\n", before,
        GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS));
    Require(std::fclose(metadata) == 0 && std::fclose(cells) == 0 && std::fclose(providers) == 0, "owned receipts close");
    std::puts("DONE exact FixedSys source investigation; no font/raster bytes exported or policy selected");
}
