#include "mac_settings_store.h"

#include <CoreFoundation/CoreFoundation.h>

#include <array>
#include <climits>
#include <cstring>
#include <limits>

namespace cse {
namespace mac_settings {
namespace {

static_assert(CHAR_BIT == 8 && sizeof(std::int32_t) == 4, "Settings integers require four octets");
static_assert(static_cast<std::uint64_t>(std::numeric_limits<CFIndex>::max()) >= UINT32_MAX,
    "The native preferences adapter requires a 64-bit CFIndex");
constexpr std::size_t IntegerBytes = 4;
constexpr unsigned int BitsPerByte = 8;
constexpr std::int32_t RecordVersion = 1;
constexpr std::size_t RecordFields = 3;
constexpr std::size_t Utf16UnitBytes = 2;
static_assert(std::numeric_limits<std::size_t>::max() / Utf16UnitBytes >= MaximumValueBytes,
    "UTF16 staging extents require native 64-bit size_t");

enum class ValueKind : std::int32_t {
    Binary,
    String
};

// These operations are synchronous and borrowed only by a stack-local Store.
// The public API exposes neither callbacks nor CoreFoundation objects.
struct PreferencesOps {
    void* context;
    CFPropertyListRef (*copyValue)(void*, CFStringRef, CFStringRef);
    void (*setValue)(void*, CFStringRef, CFPropertyListRef, CFStringRef);
    bool (*synchronize)(void*, CFStringRef);
};

template<typename T>
class ScopedCF {
public:
    explicit ScopedCF(T value) noexcept : m_value(value) {}

    ~ScopedCF() {
        if (m_value) {
            CFRelease(m_value);
        }
    }

    ScopedCF(const ScopedCF&) = delete;
    ScopedCF& operator=(const ScopedCF&) = delete;
    T Get() const noexcept { return m_value; }

private:
    T m_value;
};

CFStringRef CreateUtf8String(const char* text) noexcept {
    if (!text) {
        return nullptr;
    }
    const std::size_t length = std::strlen(text);
    if (length > MaximumValueBytes) {
        return nullptr;
    }
    return CFStringCreateWithBytes(nullptr, reinterpret_cast<const UInt8*>(text),
        static_cast<CFIndex>(length), kCFStringEncodingUTF8, false);
}

CFMutableStringRef CreateKey(const char* key) noexcept {
    if (!key || !*key) {
        return nullptr;
    }
    const std::size_t length = std::strlen(key);
    if (length > MaximumValueBytes || key[length - 1] == '\\') {
        return nullptr;
    }
    const ScopedCF<CFStringRef> source(CreateUtf8String(key));
    const ScopedCF<CFStringRef> localeName(CreateUtf8String("en_US_POSIX"));
    if (!source.Get() || !localeName.Get()) {
        return nullptr;
    }
    const ScopedCF<CFLocaleRef> locale(CFLocaleCreate(nullptr, localeName.Get()));
    if (!locale.Get()) {
        return nullptr;
    }
    CFMutableStringRef normalized = CFStringCreateMutableCopy(nullptr, 0, source.Get());
    if (normalized) {
        // Canonical equivalence and Unicode case folding are explicit native policy.
        // No diacritic/width folding, path splitting or separator replacement occurs.
        CFStringNormalize(normalized, kCFStringNormalizationFormC);
        CFStringFold(normalized, kCFCompareCaseInsensitive, locale.Get());
        CFStringNormalize(normalized, kCFStringNormalizationFormC);
    }
    return normalized;
}

std::array<UInt8, IntegerBytes> EncodeInteger(std::int32_t value) noexcept {
    const std::uint32_t bits = static_cast<std::uint32_t>(value);
    std::array<UInt8, IntegerBytes> bytes{{}};
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<UInt8>(bits >> (index * BitsPerByte));
    }
    return bytes;
}

std::int32_t DecodeInteger(const std::array<UInt8, IntegerBytes>& bytes) noexcept {
    std::uint32_t bits = 0;
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bits |= static_cast<std::uint32_t>(bytes[index]) << (index * BitsPerByte);
    }
    if (bits <= static_cast<std::uint32_t>(INT32_MAX)) {
        return static_cast<std::int32_t>(bits);
    }
    return -1 - static_cast<std::int32_t>(UINT32_MAX - bits);
}

CFDictionaryRef CreateRecord(const void* source, std::size_t bytes, ValueKind kind) noexcept {
    const ScopedCF<CFDataRef> payload(CFDataCreate(nullptr, static_cast<const UInt8*>(source),
        static_cast<CFIndex>(bytes)));
    const ScopedCF<CFStringRef> versionName(CreateUtf8String("version"));
    const ScopedCF<CFStringRef> kindName(CreateUtf8String("kind"));
    const ScopedCF<CFStringRef> payloadName(CreateUtf8String("payload"));
    const std::int32_t encodedKind = static_cast<std::int32_t>(kind);
    const ScopedCF<CFNumberRef> version(CFNumberCreate(nullptr, kCFNumberSInt32Type, &RecordVersion));
    const ScopedCF<CFNumberRef> type(CFNumberCreate(nullptr, kCFNumberSInt32Type, &encodedKind));
    if (!payload.Get() || !versionName.Get() || !kindName.Get() || !payloadName.Get() ||
        !version.Get() || !type.Get()) {
        return nullptr;
    }
    std::array<const void*, RecordFields> keys{{versionName.Get(), kindName.Get(), payloadName.Get()}};
    std::array<const void*, RecordFields> values{{version.Get(), type.Get(), payload.Get()}};
    return CFDictionaryCreate(nullptr, keys.data(), values.data(), static_cast<CFIndex>(keys.size()),
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

bool IntegerField(CFTypeRef field, std::int32_t& value) noexcept {
    if (!field || CFGetTypeID(field) != CFNumberGetTypeID()) {
        return false;
    }
    const CFNumberRef number = static_cast<CFNumberRef>(field);
    return !CFNumberIsFloatType(number) && CFNumberGetValue(number, kCFNumberSInt32Type, &value);
}

CFDataRef RecordPayload(CFPropertyListRef record, ValueKind& kind) noexcept {
    if (!record || CFGetTypeID(record) != CFDictionaryGetTypeID()) {
        return nullptr;
    }
    const CFDictionaryRef dictionary = static_cast<CFDictionaryRef>(record);
    if (CFDictionaryGetCount(dictionary) != static_cast<CFIndex>(RecordFields)) {
        return nullptr;
    }
    const ScopedCF<CFStringRef> versionName(CreateUtf8String("version"));
    const ScopedCF<CFStringRef> kindName(CreateUtf8String("kind"));
    const ScopedCF<CFStringRef> payloadName(CreateUtf8String("payload"));
    if (!versionName.Get() || !kindName.Get() || !payloadName.Get()) {
        return nullptr;
    }
    std::int32_t version = 0;
    std::int32_t type = 0;
    if (!IntegerField(CFDictionaryGetValue(dictionary, versionName.Get()), version) || version != RecordVersion ||
        !IntegerField(CFDictionaryGetValue(dictionary, kindName.Get()), type) ||
        (type != static_cast<std::int32_t>(ValueKind::Binary) && type != static_cast<std::int32_t>(ValueKind::String))) {
        return nullptr;
    }
    const CFTypeRef payload = CFDictionaryGetValue(dictionary, payloadName.Get());
    if (!payload || CFGetTypeID(payload) != CFDataGetTypeID()) {
        return nullptr;
    }
    kind = static_cast<ValueKind>(type);
    return static_cast<CFDataRef>(payload);
}

CFStringRef DecodeString(CFDataRef payload) noexcept {
    const CFIndex length = CFDataGetLength(payload);
    if (length <= 0) {
        return nullptr;
    }
    const UInt8* bytes = CFDataGetBytePtr(payload);
    if (std::memchr(bytes, 0, static_cast<std::size_t>(length)) != bytes + length - 1) {
        return nullptr;
    }
    return CFStringCreateWithBytes(nullptr, bytes, length - 1, kCFStringEncodingUTF8, false);
}

class Store {
public:
    Store(const char* domain, PreferencesOps operations) noexcept : m_domain(domain), m_operations(operations) {}

    bool ReadBytes(const char* key, void* destination, std::size_t capacity,
        std::size_t* requiredBytes = nullptr) const noexcept {
        if (requiredBytes) {
            *requiredBytes = 0;
        }
        if (capacity > MaximumValueBytes) {
            return false;
        }
        const ScopedCF<CFStringRef> domain(CreateUtf8String(m_domain));
        const ScopedCF<CFMutableStringRef> name(CreateKey(key));
        if (!domain.Get() || !name.Get() || !m_operations.synchronize(m_operations.context, domain.Get())) {
            return false;
        }
        const ScopedCF<CFPropertyListRef> value(
            m_operations.copyValue(m_operations.context, name.Get(), domain.Get()));
        ValueKind kind = ValueKind::Binary;
        const CFDataRef data = RecordPayload(value.Get(), kind);
        if (!data) {
            return false;
        }
        const CFIndex length = CFDataGetLength(data);
        if (length < 0 || static_cast<std::uint64_t>(length) > MaximumValueBytes) {
            return false;
        }
        const std::size_t bytes = static_cast<std::size_t>(length);
        const ScopedCF<CFStringRef> string(kind == ValueKind::String ? DecodeString(data) : nullptr);
        if (kind == ValueKind::String && !string.Get()) {
            return false;
        }
        if (requiredBytes) {
            *requiredBytes = bytes;
        }
        if (!destination) {
            return true;
        }
        if (capacity < bytes) {
            return false;
        }
        if (string.Get()) {
            const std::size_t units = static_cast<std::size_t>(CFStringGetLength(string.Get()));
            const std::size_t wideBytes = (units + 1) * Utf16UnitBytes;
            if (capacity >= wideBytes) {
                // Measured RegQueryValueExA/ACP65001 behavior: a large-enough
                // buffer first receives UTF16LE, then its UTF8 prefix replaces it.
                // The remaining UTF16 bytes are observable even beyond requiredBytes.
                UInt8* output = static_cast<UInt8*>(destination);
                for (std::size_t index = 0; index < units; ++index) {
                    const UniChar unit = CFStringGetCharacterAtIndex(string.Get(), static_cast<CFIndex>(index));
                    output[index * Utf16UnitBytes] = static_cast<UInt8>(unit);
                    output[index * Utf16UnitBytes + 1] = static_cast<UInt8>(unit >> BitsPerByte);
                }
                output[units * Utf16UnitBytes] = 0;
                output[units * Utf16UnitBytes + 1] = 0;
            }
        }
        if (bytes != 0) {
            std::memcpy(destination, CFDataGetBytePtr(data), bytes);
        }
        return true;
    }

    bool WriteBytes(const char* key, const void* source, std::size_t bytes) const noexcept {
        return WriteValue(key, source, bytes, ValueKind::Binary);
    }

    bool WriteValue(const char* key, const void* source, std::size_t bytes, ValueKind kind) const noexcept {
        if (bytes > MaximumValueBytes || (!source && bytes != 0)) {
            return false;
        }
        const ScopedCF<CFStringRef> domain(CreateUtf8String(m_domain));
        const ScopedCF<CFMutableStringRef> name(CreateKey(key));
        if (!domain.Get() || !name.Get()) {
            return false;
        }
        const ScopedCF<CFDictionaryRef> value(CreateRecord(source, bytes, kind));
        if (!value.Get()) {
            return false;
        }
        m_operations.setValue(m_operations.context, name.Get(), value.Get(), domain.Get());
        // SetValue has no error result. Sync is the acknowledgement boundary;
        // failure does not promise rollback of CoreFoundation's pending cache.
        return m_operations.synchronize(m_operations.context, domain.Get());
    }

    bool WriteString(const char* key, const char* value) const noexcept {
        if (!value) {
            return false;
        }
        const std::size_t bytes = std::strlen(value);
        if (bytes >= MaximumValueBytes) {
            return false;
        }
        const ScopedCF<CFStringRef> string(CreateUtf8String(value));
        return string.Get() && WriteValue(key, value, bytes + 1, ValueKind::String);
    }

    bool ReadInt32(const char* key, std::int32_t& value) const noexcept {
        std::array<UInt8, IntegerBytes> bytes = EncodeInteger(value);
        if (!ReadBytes(key, bytes.data(), bytes.size())) {
            return false;
        }
        value = DecodeInteger(bytes);
        return true;
    }

    bool WriteInt32(const char* key, std::int32_t value) const noexcept {
        const std::array<UInt8, IntegerBytes> bytes = EncodeInteger(value);
        return WriteBytes(key, bytes.data(), bytes.size());
    }

private:
    const char* m_domain;
    PreferencesOps m_operations;
};

CFPropertyListRef CopyPreference(void*, CFStringRef key, CFStringRef domain) {
    return CFPreferencesCopyValue(key, domain, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
}

void SetPreference(void*, CFStringRef key, CFPropertyListRef value, CFStringRef domain) {
    CFPreferencesSetValue(key, value, domain, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
}

bool SynchronizePreferences(void*, CFStringRef domain) {
    return CFPreferencesSynchronize(domain, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
}

PreferencesOps NativeOperations() noexcept {
    return {nullptr, CopyPreference, SetPreference, SynchronizePreferences};
}

} // namespace

bool ReadBytes(const char* key, void* destination, std::size_t capacity, std::size_t* requiredBytes) noexcept {
    return Store(ProductDomain, NativeOperations()).ReadBytes(key, destination, capacity, requiredBytes);
}

bool WriteBytes(const char* key, const void* source, std::size_t bytes) noexcept {
    return Store(ProductDomain, NativeOperations()).WriteBytes(key, source, bytes);
}

bool WriteString(const char* key, const char* value) noexcept {
    return Store(ProductDomain, NativeOperations()).WriteString(key, value);
}

bool ReadInt32(const char* key, std::int32_t& value) noexcept {
    return Store(ProductDomain, NativeOperations()).ReadInt32(key, value);
}

bool WriteInt32(const char* key, std::int32_t value) noexcept {
    return Store(ProductDomain, NativeOperations()).WriteInt32(key, value);
}

} // namespace mac_settings
} // namespace cse
