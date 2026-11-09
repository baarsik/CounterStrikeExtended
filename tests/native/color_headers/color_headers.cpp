#include <array>
#include <cstdint>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

#include "vgui/IScheme.h"
#include "../../../GoldSrc-SDK/vgui2/controls/Panel.h"

template <typename T, typename = void>
struct HasEngineColorAccessors : std::false_type {};

template <typename T>
struct HasEngineColorAccessors<T, std::void_t<decltype(std::declval<const T&>().r())>> : std::true_type {};

static_assert(sizeof(Color) == 4 && alignof(Color) == 1, "RGBA byte storage is unchanged");
#if defined(CSE_EXPECT_ENGINE_COLOR)
static_assert(HasEngineColorAccessors<Color>::value, "Keep the engine/CS color declaration binding");
static_assert(std::is_trivially_copy_assignable<Color>::value, "Keep the engine's existing copy contract");
#else
static_assert(!HasEngineColorAccessors<Color>::value, "Keep the distinct retained VGUI2 declaration binding");
static_assert(!std::is_trivially_copy_assignable<Color>::value, "Keep the VGUI2 existing copy contract");
static_assert(std::is_same<decltype(std::declval<const Color&>()[0]), const unsigned char&>::value,
    "Keep the canonical VGUI2 const channel accessor");
#endif

int main() {
    constexpr std::size_t kRgbaBytes = sizeof(Color);
    alignas(std::uint32_t) std::array<unsigned char, kRgbaBytes + 2> bytes{};
    auto* destination = new (bytes.data() + 1) Color();
    const Color source(0x12, 0x34, 0x56, 0x78);
    const Color copied(source);
    *destination = copied;
    if (std::memcmp(destination, &source, sizeof(source)) != 0 || bytes.front() != 0 || bytes.back() != 0) {
        return 1;
    }
    *destination = *destination;
    return std::memcmp(destination, &source, sizeof(source)) == 0 ? 0 : 1;
}
