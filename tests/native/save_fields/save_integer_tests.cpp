#include "../../../GoldSrc-SDK/common/save_field_storage.h"
#include "../../../GoldSrc-SDK/common/legacy_save_integer.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "Check failed at line %d: %s\n", __LINE__, #condition); \
    return 1; \
} } while (false)

enum class WideSignedPhase : std::int64_t { Pending = -1 };

int main() {
    using namespace cse::save;
    static_assert(NativeStorageTraits<WideSignedPhase>::Flags == NativeSigned);
    static_assert(NativeStorageTraits<bool>::Stride == sizeof(bool));
    static_assert(NativeStorageTraits<bool>::Flags == NativeBoolean);
    static_assert(NativeStorageTraits<short>::Flags == NativeSigned);
    static_assert(NativeStorageTraits<unsigned short>::Flags == 0);
    static_assert(NativeStorageTraits<void *[8]>::Stride == sizeof(void *));
    static_assert(NativeStorageTraits<float[2][3]>::Stride == sizeof(float[3]));
    std::array<unsigned char, LegacyIntegerBytes> wire{};
    WideSignedPhase phase = WideSignedPhase::Pending;
    CHECK(EncodeLegacyInteger(&phase, sizeof(phase), NativeStorageTraits<WideSignedPhase>::Flags, wire.data()));
    CHECK((wire == std::array<unsigned char, LegacyIntegerBytes>{0xff, 0xff, 0xff, 0xff}));
    std::int64_t negative = -1;
    CHECK(EncodeLegacyInteger(&negative, sizeof(negative), NativeSigned, wire.data()));
    CHECK((wire == std::array<unsigned char, LegacyIntegerBytes>{0xff, 0xff, 0xff, 0xff}));
    std::int64_t restored = 0;
    CHECK(DecodeLegacyInteger(wire.data(), &restored, sizeof(restored), NativeSigned));
    CHECK(restored == -1);
    std::uint64_t unsignedRestored = 0;
    CHECK(DecodeLegacyInteger(wire.data(), &unsignedRestored, sizeof(unsignedRestored), 0));
    CHECK(unsignedRestored == UINT64_C(4294967295));

    negative = -INT64_C(2147483648);
    CHECK(EncodeLegacyInteger(&negative, sizeof(negative), NativeSigned, wire.data()));
    CHECK((wire == std::array<unsigned char, LegacyIntegerBytes>{0, 0, 0, 0x80}));
    negative = -INT64_C(2147483649);
    const auto beforeFailure = wire;
    CHECK(!EncodeLegacyInteger(&negative, sizeof(negative), NativeSigned, wire.data()));
    CHECK(wire == beforeFailure);
    std::uint64_t tooWide = UINT64_C(4294967296);
    CHECK(!EncodeLegacyInteger(&tooWide, sizeof(tooWide), 0, wire.data()));
    CHECK(wire == beforeFailure);

    struct NarrowFields {
        std::uint32_t before = 0x12345678;
        bool defusing = true;
        bool c4 = false;
        std::uint16_t event = 0;
        std::uint32_t after = 0x87654321;
    };
    auto fields = std::make_unique<NarrowFields>();
    if constexpr (sizeof(void *) > sizeof(std::uint32_t)) {
        CHECK(reinterpret_cast<std::uintptr_t>(fields.get()) > (std::numeric_limits<std::uint32_t>::max)());
    }
    // Legacy FIELD_BOOLEAN saved four bytes, including a neighbouring bool.
    // 0x00000100 means false in the original one-byte field, not true.
    wire = {0, 1, 0, 0};
    CHECK(DecodeLegacyInteger(wire.data(), &fields->c4, sizeof(fields->c4), NativeBoolean));
    CHECK(!fields->c4 && fields->defusing);
    // An int-backed BOOL keeps all four bytes and its existing nonzero value.
    int legacyIntBoolean = 0;
    CHECK(DecodeLegacyInteger(wire.data(), &legacyIntBoolean, sizeof(legacyIntBoolean), NativeSigned));
    CHECK(legacyIntBoolean == 256);
    CHECK(EncodeLegacyInteger(&fields->c4, sizeof(fields->c4), NativeBoolean, wire.data()));
    CHECK((wire == std::array<unsigned char, LegacyIntegerBytes>{0, 0, 0, 0}));
    wire = {1, 0xff, 0xff, 0xff};
    CHECK(DecodeLegacyInteger(wire.data(), &fields->c4, sizeof(fields->c4), NativeBoolean));
    CHECK(fields->c4 && fields->defusing);
    CHECK(EncodeLegacyInteger(&fields->c4, sizeof(fields->c4), NativeBoolean, wire.data()));
    CHECK((wire == std::array<unsigned char, LegacyIntegerBytes>{1, 0, 0, 0}));
    // Legacy FIELD_INTEGER on unsigned short included unrelated high bytes.
    wire = {0x34, 0x12, 0xe0, 0xfe};
    CHECK(DecodeLegacyInteger(wire.data(), &fields->event, sizeof(fields->event), 0));
    CHECK(fields->event == 0x1234);
    CHECK(fields->c4 && fields->defusing);
    CHECK(fields->before == 0x12345678 && fields->after == 0x87654321);
    CHECK(EncodeLegacyInteger(&fields->event, sizeof(fields->event), 0, wire.data()));
    CHECK((wire == std::array<unsigned char, LegacyIntegerBytes>{0x34, 0x12, 0, 0}));
    signed char signedByte = -1;
    CHECK(EncodeLegacyInteger(&signedByte, sizeof(signedByte), NativeSigned, wire.data()));
    CHECK((wire == std::array<unsigned char, LegacyIntegerBytes>{0xff, 0xff, 0xff, 0xff}));
    signedByte = 0;
    CHECK(DecodeLegacyInteger(wire.data(), &signedByte, sizeof(signedByte), NativeSigned));
    CHECK(signedByte == -1);
    CHECK(!DecodeLegacyInteger(wire.data(), &restored, 0, NativeSigned));
    CHECK(!DecodeLegacyInteger(wire.data(), &restored, 16, NativeSigned));
    CHECK(!DecodeLegacyInteger(wire.data(), &restored, sizeof(restored), NativeBoolean));
    CHECK(!EncodeLegacyInteger(&restored, 3, 0, wire.data()));
    return 0;
}
