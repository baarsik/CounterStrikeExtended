#include "common.h"
#include "tmessage.h"
#include "localized_titles.h"
#include "filesystem.h"
#include "mem.h"
#include "../common/resource_text.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

extern client_textmessage_t* gMessageTable;
extern int gMessageTableCount;

namespace {
cse::localization::TitleSnapshots titleSnapshots;
std::vector<std::pair<client_textmessage_t*, int>> retainedLegacyTables;

bool ReadTitles(IFileSystem* filesystem, std::string& text) {
    constexpr unsigned int MaximumTitleFileBytes = 4 * 1024 * 1024;
    if (!filesystem) {
        return false;
    }
    FileHandle_t file = filesystem->Open("titles.txt", "rb", nullptr);
    if (!file) {
        return false;
    }
    const unsigned int size = filesystem->Size(file);
    bool valid = false;
    if (size > 0 && size <= MaximumTitleFileBytes) {
        std::vector<byte> bytes(size);
        if (filesystem->Read(bytes.data(), static_cast<int>(size), file) == static_cast<int>(size)) {
            valid = cse::resource_text::Decode(bytes.data(), bytes.size(), text);
        }
    }
    filesystem->Close(file);
    return valid;
}

bool Contains(const client_textmessage_t* table, int count, const client_textmessage_t* message) {
    const auto address = reinterpret_cast<std::uintptr_t>(message);
    const auto begin = reinterpret_cast<std::uintptr_t>(table);
    return table && message && count > 0 && address >= begin
        && address - begin < static_cast<std::size_t>(count) * sizeof(client_textmessage_t)
        && (address - begin) % sizeof(client_textmessage_t) == 0;
}
} // namespace

bool TextMessagePrepareLanguage(IFileSystem* candidate, IFileSystem* baseline) {
    std::string english;
    std::string selected;
    return ReadTitles(baseline, english) && ReadTitles(candidate, selected)
        && titleSnapshots.Prepare(english, selected);
}

void TextMessageCommitLanguage() {
    if (gMessageTable && !titleSnapshots.IsCatalogue(gMessageTable)) {
        retainedLegacyTables.emplace_back(gMessageTable, gMessageTableCount);
    }
    titleSnapshots.Commit();
    auto* snapshot = titleSnapshots.Active();
    gMessageTable = snapshot ? snapshot->Data() : nullptr;
    gMessageTableCount = snapshot ? snapshot->Count() : 0;
}

void TextMessageDiscardLanguage() {
    titleSnapshots.Discard();
}

bool TextMessageIsCatalogue(const client_textmessage_t* message) {
    if (!message) {
        return false;
    }
    if (Contains(gMessageTable, gMessageTableCount, message) || titleSnapshots.IsCatalogue(message)) {
        return true;
    }
    for (const auto& retained : retainedLegacyTables) {
        if (Contains(retained.first, retained.second, message)) {
            return true;
        }
    }
    return false;
}

void TextMessageRetireLanguageTables() {
    if (titleSnapshots.IsCatalogue(gMessageTable)) {
        gMessageTable = nullptr;
    }
    titleSnapshots.Clear();
    for (const auto& retained : retainedLegacyTables) {
        Mem_Free(retained.first);
    }
    retainedLegacyTables.clear();
}
