#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <new>
#include <type_traits>
#include <utility>

#define private public
#include "vgui2/src/LocalizedStringTable.h"
#undef private
#include "hud.h"

using Index = vgui2::StringIndex_t;
using Record = CLocalizedStringTable::localizedstring_t;
using Lookup = decltype(std::declval<CLocalizedStringTable>().m_Lookup);
using HudFont = decltype(CHudVGUI2Print::m_hudfont);

struct OriginalRecord {
    unsigned long nameIndex;
    unsigned long valueIndex;
    CUtlSymbol filename;
};

static_assert(sizeof(Index) == 4, "Windows IDs remain four bytes");
static_assert(std::is_same<Index, unsigned long>::value, "exact public ID underlying type");
static_assert(std::is_same<Lookup, CUtlRBTree<Record, unsigned long>>::value, "exact original tree instantiation");
static_assert(std::is_same<HudFont, unsigned long>::value, "exact original HUD field type");
static_assert(sizeof(Record) == sizeof(OriginalRecord), "original localization record size");
static_assert(alignof(Record) == alignof(OriginalRecord), "original localization record alignment");
static_assert(offsetof(Record, nameIndex) == offsetof(OriginalRecord, nameIndex), "original name offset");
static_assert(offsetof(Record, valueIndex) == offsetof(OriginalRecord, valueIndex), "original value offset");
static_assert(offsetof(Record, filename) == offsetof(OriginalRecord, filename), "original filename offset");
static_assert(std::is_same<decltype(&CLocalizedStringTable::FindIndex),
    unsigned long (CLocalizedStringTable::*)(const char*)>::value, "exact public find method signature");
static_assert(std::is_same<decltype(&CLocalizedStringTable::GetNextStringIndex),
    unsigned long (CLocalizedStringTable::*)(unsigned long)>::value, "exact public iteration method signature");
static_assert(INVALID_LOCALIZE_STRING_INDEX == UINT32_MAX, "original invalid ID");
static_assert(sizeof(vgui2::OldStringIndex_t) == 2, "original legacy adapter type width");

int main() {
    std::printf("ptr=%zu ID=%zu old=%zu record=%zu/%zu tree=%zu/%zu table=%zu/%zu HUD=%zu/%zu font=%zu\n",
        sizeof(void*), sizeof(Index), sizeof(vgui2::OldStringIndex_t), sizeof(Record), alignof(Record),
        sizeof(Lookup), alignof(Lookup), sizeof(CLocalizedStringTable), alignof(CLocalizedStringTable),
        sizeof(CHudVGUI2Print), alignof(CHudVGUI2Print), sizeof(HudFont));
    return EXIT_SUCCESS;
}
