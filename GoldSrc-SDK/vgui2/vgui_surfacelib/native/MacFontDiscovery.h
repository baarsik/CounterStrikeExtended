#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

struct FT_LibraryRec_;

namespace cse::fonts {

struct FontFile {
    std::string path;
    std::string postScriptName;
    long faceIndex = 0;
};

// Names and paths use UTF-8. Results own their strings; no CoreText objects escape.
// Unknown names fail rather than silently selecting an unrelated family.
// Family names apply weight/italic selection; PostScript names identify an exact face.
bool FindMacFontFile(FT_LibraryRec_* library, const char* name, int weight, bool italic, FontFile& result);

// Discover fallback for exactly one well-formed UTF-16 scalar. No shaping or layout is performed here.
bool FindMacFontFallback(FT_LibraryRec_* library, const char* name, int weight, bool italic,
    const std::uint16_t* text, std::size_t length, FontFile& result);

}
