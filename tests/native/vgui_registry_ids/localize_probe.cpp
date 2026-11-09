#include <cstdint>
#include <cstdio>
#include <type_traits>

#if defined(CSE_LOCALIZE_ENGINE)
#include "engine/ILocalize.h"
#elif defined(CSE_LOCALIZE_CS)
#include "cstrike/public/vgui/ILocalize.h"
#else
#include "vgui2/src/public/vgui/ILocalize.h"
#endif

using Index = vgui2::StringIndex_t;
using FindSignature = Index (vgui2::ILocalize::*)(const char*);
using NameSignature = const char* (vgui2::ILocalize::*)(Index);
using ValueSignature = wchar_t* (vgui2::ILocalize::*)(Index);
using FirstSignature = Index (vgui2::ILocalize::*)();
using NextSignature = Index (vgui2::ILocalize::*)(Index);
using SetSignature = void (vgui2::ILocalize::*)(Index, wchar_t*);
using ConstructSignature = void (vgui2::ILocalize::*)(wchar_t*, int, Index, KeyValues*);

static_assert(sizeof(Index) == sizeof(std::uint32_t), "localization IDs retain 32 bits");
static_assert(std::is_unsigned<Index>::value, "localization IDs remain unsigned");
static_assert(std::is_same<decltype(&vgui2::ILocalize::FindIndex), FindSignature>::value, "find signature");
static_assert(std::is_same<decltype(&vgui2::ILocalize::GetNameByIndex), NameSignature>::value, "name signature");
static_assert(std::is_same<decltype(&vgui2::ILocalize::GetValueByIndex), ValueSignature>::value, "value signature");
static_assert(std::is_same<decltype(&vgui2::ILocalize::GetFirstStringIndex), FirstSignature>::value, "first signature");
static_assert(std::is_same<decltype(&vgui2::ILocalize::GetNextStringIndex), NextSignature>::value, "next signature");
static_assert(std::is_same<decltype(&vgui2::ILocalize::SetValueByIndex), SetSignature>::value, "set signature");
static_assert(std::is_same<decltype(static_cast<ConstructSignature>(&vgui2::ILocalize::ConstructString)),
    ConstructSignature>::value, "indexed construction signature");
static_assert(static_cast<Index>(-1) == UINT32_MAX, "invalid localization ID retains all 32 bits");
#ifdef INVALID_STRING_INDEX
static_assert(INVALID_STRING_INDEX == UINT32_MAX, "published invalid ID retains all 32 bits");
#endif
#if defined(_WIN32)
static_assert(std::is_same<Index, unsigned long>::value, "Windows retains the exact underlying type");
#else
static_assert(std::is_same<Index, std::uint32_t>::value, "portable IDs use explicit 32-bit storage");
#endif

int main() {
    std::printf("localization ID bytes=%zu; unsigned, sentinel and public signatures verified\n", sizeof(Index));
    return 0;
}
