#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <new>
#include <type_traits>
#include <utility>

// Test-only access to the actual private record and container types. Their implementation is unmodified.
#define private public
#include "vgui2/src/LocalizedStringTable.h"
#undef private

using Index = vgui2::StringIndex_t;
using Record = CLocalizedStringTable::localizedstring_t;
using Lookup = decltype(std::declval<CLocalizedStringTable>().m_Lookup);
using Names = decltype(std::declval<CLocalizedStringTable>().m_Names);
using Values = decltype(std::declval<CLocalizedStringTable>().m_Values);
using OldIndex = vgui2::OldStringIndex_t;
static_assert(sizeof(Index) == 4, "the table repeats the same 32-bit public ID declaration");
static_assert(sizeof(OldIndex) == 2, "the historical adapter remains 16 bits");
static_assert(sizeof(Record::nameIndex) == 4 && sizeof(Record::valueIndex) == 4, "pool offsets retain 32 bits");
static_assert(std::is_same<Lookup::IndexType_t, Index>::value, "tree slots use the public ID type");
static_assert(INVALID_LOCALIZE_STRING_INDEX == UINT32_MAX, "table invalid ID retains all 32 bits");
#if defined(_WIN32)
static_assert(std::is_same<Lookup, CUtlRBTree<Record, unsigned long>>::value, "Windows exact tree type retained");
#endif

namespace {

void Require(bool condition, const char* description) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        std::exit(EXIT_FAILURE);
    }
}

void RequireNativeAllocation(const void* allocation) {
    constexpr std::uint64_t Maximum32BitAddress = UINT32_MAX;
    if (sizeof(void*) > sizeof(std::uint32_t)) {
        Require(reinterpret_cast<std::uintptr_t>(allocation) > Maximum32BitAddress,
            "64-bit fixture must actually allocate above 4 GiB");
    }
}

}

// Controlled class binding uses the actual record/tree/vector types and extracted, unchanged method bodies.
// It omits unrelated file/surface dependencies and does not claim to instantiate the complete production class.
class ControlledTable {
public:
    using localizedstring_t = Record;
    ControlledTable() : m_Lookup(0, 0, SymLess) {
    }
    static bool SymLess(const Record& left, const Record& right);
    Index FindIndex(const char* name);
    const char* GetNameByIndex(Index index);
    wchar_t* GetValueByIndex(Index index);
    Index GetFirstStringIndex();
    Index GetNextStringIndex(Index index);
    Lookup m_Lookup;
    Names m_Names;
    Values m_Values;
};

struct LessContext {
    ControlledTable* m_pTable;
    const char* m_pUserString;
};
LessContext g_LessCtx = {};
using vgui2::StringIndex_t;
#include "retained_table_methods.inc"

namespace LegacyFixture {

using vgui2::OldStringIndex_t;

struct ModernBindings {
    Index returned = 0;
    Index received = 0;
    const char* name = "name";
    wchar_t value[2] = L"v";
    Index FindIndex(const char*) {
        return returned;
    }

    Index GetFirstStringIndex() {
        return returned;
    }

    Index GetNextStringIndex(Index index) {
        received = index;
        return returned;
    }

    const char* GetNameByIndex(Index index) {
        received = index;
        return name;
    }

    wchar_t* GetValueByIndex(Index index) {
        received = index;
        return value;
    }

    const char* GetFileNameByIndex(Index index) {
        received = index;
        return name;
    }

    void SetValueByIndex(Index index, wchar_t*) {
        received = index;
    }
};
ModernBindings g_StringTable;

class CLocalizedStringTable_VGUI_Localize002 {
public:
    OldStringIndex_t FindIndex(const char* tokenName);
    const char* GetNameByIndex(OldStringIndex_t index);
    wchar_t* GetValueByIndex(OldStringIndex_t index);
    OldStringIndex_t GetFirstStringIndex();
    OldStringIndex_t GetNextStringIndex(OldStringIndex_t index);
    void SetValueByIndex(OldStringIndex_t index, wchar_t* newValue);
    const char* GetFileNameByIndex(OldStringIndex_t index);
};

// Only modern-table responses are controlled; all seven adapter bodies are extracted from the retained CPP.
#include "retained_legacy_methods.inc"

void TestLegacyAdapter() {
    CLocalizedStringTable_VGUI_Localize002 adapter;
    constexpr std::uint32_t Ids[] = {0, 1, 0xffff, 0x10000, 0x7fffffff, 0x80000000, 0xfffffffe, UINT32_MAX};
    constexpr Index ForwardedOldInvalid = 0xffff;
    for (std::uint32_t id : Ids) {
        g_StringTable.returned = id;
        const OldIndex expected = static_cast<OldIndex>(id);
        Require(adapter.FindIndex("token") == expected, "legacy find retains low 16 bits");
        Require(adapter.GetFirstStringIndex() == expected, "legacy first retains low 16 bits");
        Require(adapter.GetNextStringIndex(static_cast<OldIndex>(ForwardedOldInvalid)) == expected,
            "legacy next retains low 16 bits");
        Require(g_StringTable.received == ForwardedOldInvalid, "legacy next forwards 0xffff without normalization");
    }
    const OldIndex oldInvalid = static_cast<OldIndex>(ForwardedOldInvalid);
    Require(adapter.GetNameByIndex(oldInvalid) == g_StringTable.name, "legacy name forwards result");
    Require(g_StringTable.received == ForwardedOldInvalid, "legacy name forwards 16-bit ID");
    Require(adapter.GetValueByIndex(oldInvalid) == g_StringTable.value, "legacy value forwards result");
    Require(g_StringTable.received == ForwardedOldInvalid, "legacy value forwards 16-bit ID");
    Require(adapter.GetFileNameByIndex(oldInvalid) == g_StringTable.name, "legacy filename forwards result");
    Require(g_StringTable.received == ForwardedOldInvalid, "legacy filename forwards 16-bit ID");
    adapter.SetValueByIndex(oldInvalid, g_StringTable.value);
    Require(g_StringTable.received == ForwardedOldInvalid, "legacy set forwards 16-bit ID");
}

}

int main() {
    constexpr std::uint32_t Ids[] = {0, 1, 0xffff, 0x10000, 0x7fffffff, 0x80000000, 0xfffffffe, UINT32_MAX};
    for (std::uint32_t id : Ids) {
        Record record;
        record.nameIndex = id;
        record.valueIndex = id;
        Require(record.nameIndex == id && record.valueIndex == id, "record preserves all ID/offset bits");
    }

    ControlledTable table;
    constexpr char NamePool[] = "gamma\0alpha\0beta";
    constexpr wchar_t ValuePool[] = L"three\0one\0two";
    const int namesStart = table.m_Names.AddMultipleToTail(sizeof(NamePool));
    const int valuesStart = table.m_Values.AddMultipleToTail(sizeof(ValuePool) / sizeof(ValuePool[0]));
    Require(namesStart == 0 && valuesStart == 0, "initial pool offsets are zero");
    std::memcpy(table.m_Names.Base(), NamePool, sizeof(NamePool));
    std::memcpy(table.m_Values.Base(), ValuePool, sizeof(ValuePool));
    RequireNativeAllocation(table.m_Names.Base());
    RequireNativeAllocation(table.m_Values.Base());
    constexpr Index NameOffsets[] = {0, 6, 12};
    constexpr Index ValueOffsets[] = {0, 6, 10};
    Index inserted[3] = {};
    g_LessCtx.m_pTable = &table;
    for (std::size_t position = 0; position < sizeof(inserted) / sizeof(inserted[0]); ++position) {
        Record record;
        record.nameIndex = NameOffsets[position];
        record.valueIndex = ValueOffsets[position];
        inserted[position] = table.m_Lookup.Insert(record);
        Require(inserted[position] != INVALID_LOCALIZE_STRING_INDEX, "real tree insertion returns a valid slot");
    }
    RequireNativeAllocation(&table.m_Lookup[inserted[0]]);
    Require(table.FindIndex(nullptr) == 0, "retained null-name behavior remains zero");
    Require(table.FindIndex("#ALPHA") == inserted[1], "retained lookup strips # and ignores ASCII case");
    Require(table.FindIndex("missing") == INVALID_LOCALIZE_STRING_INDEX, "real tree missing result is 0xffffffff");
    Require(table.GetValueByIndex(INVALID_LOCALIZE_STRING_INDEX) == nullptr, "retained invalid-value check remains");
    const char* expectedOrder[] = {"alpha", "beta", "gamma"};
    const wchar_t* expectedValues[] = {L"one", L"two", L"three"};
    Index index = table.GetFirstStringIndex();
    for (std::size_t position = 0; position < sizeof(expectedOrder) / sizeof(expectedOrder[0]); ++position) {
        Require(index != INVALID_LOCALIZE_STRING_INDEX, "in-order traversal has the expected element");
        Require(std::strcmp(table.GetNameByIndex(index), expectedOrder[position]) == 0, "name pool offsets and order");
        Require(std::wcscmp(table.GetValueByIndex(index), expectedValues[position]) == 0,
            "wchar element offsets and values");
        index = table.GetNextStringIndex(index);
    }
    Require(index == INVALID_LOCALIZE_STRING_INDEX, "in-order end retains invalid sentinel");
    LegacyFixture::TestLegacyAdapter();
    std::printf("ID=%zu old=%zu record=%zu tree=%zu; high native pools/tree, retained lookup and adapter verified\n",
        sizeof(Index), sizeof(OldIndex), sizeof(Record), sizeof(Lookup));
    return EXIT_SUCCESS;
}
