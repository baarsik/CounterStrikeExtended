#include "../../../GoldSrc-SDK/common/mac_settings_store.h"
#include <type_traits>

using ReadSignature = bool (*)(const char*, void*, std::size_t, std::size_t*);
using WriteSignature = bool (*)(const char*, const void*, std::size_t);
static_assert(std::is_same<decltype(&cse::mac_settings::ReadBytes), ReadSignature>::value, "Caller buffer API changed");
static_assert(std::is_same<decltype(&cse::mac_settings::WriteBytes), WriteSignature>::value, "Byte writer API changed");
static_assert(cse::mac_settings::MaximumValueBytes == UINT32_MAX, "DWORD payload bound changed");

int main() {
    return 0;
}
