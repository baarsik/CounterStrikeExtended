#include <CoreFoundation/CoreFoundation.h>

namespace {
int PersistentOperationAttempts = 0;
CFPropertyListRef BlockedCopy(CFStringRef, CFStringRef, CFStringRef, CFStringRef) {
    ++PersistentOperationAttempts;
    return nullptr;
}
void BlockedSet(CFStringRef, CFPropertyListRef, CFStringRef, CFStringRef, CFStringRef) {
    ++PersistentOperationAttempts;
}
Boolean BlockedSync(CFStringRef, CFStringRef, CFStringRef) {
    ++PersistentOperationAttempts;
    return false;
}
} // namespace

// Model binaries cannot call CFPreferences even if a regression selects native ops.
#define CFPreferencesCopyValue BlockedCopy
#define CFPreferencesSetValue BlockedSet
#define CFPreferencesSynchronize BlockedSync
#include "../../../src/common/mac_settings_store.cpp"
#undef CFPreferencesCopyValue
#undef CFPreferencesSetValue
#undef CFPreferencesSynchronize

#include <algorithm>
#include <array>
#include <climits>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>
#include <utility>
#include <cstdio>

namespace {

using namespace cse::mac_settings;

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

struct Model {
    CFMutableDictionaryRef values = CFDictionaryCreateMutable(nullptr, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFStringRef capturedKey = nullptr;
    CFStringRef capturedDomain = nullptr;
    CFPropertyListRef capturedValue = nullptr;
    bool syncSuccess = true;
    bool copySuccess = true;
    int copies = 0;
    int sets = 0;
    int syncs = 0;

    ~Model() {
        if (capturedKey) {
            CFRelease(capturedKey);
        }
        if (capturedDomain) {
            CFRelease(capturedDomain);
        }
        if (capturedValue) {
            CFRelease(capturedValue);
        }
        CFRelease(values);
    }

    static CFPropertyListRef Copy(void* context, CFStringRef key, CFStringRef) {
        Model& model = *static_cast<Model*>(context);
        ++model.copies;
        const void* value = CFDictionaryGetValue(model.values, key);
        return model.copySuccess && value ? CFRetain(value) : nullptr;
    }

    static void Set(void* context, CFStringRef key, CFPropertyListRef value, CFStringRef domain) {
        Model& model = *static_cast<Model*>(context);
        ++model.sets;
        if (model.capturedKey) {
            CFRelease(model.capturedKey);
        }
        if (model.capturedDomain) {
            CFRelease(model.capturedDomain);
        }
        if (model.capturedValue) {
            CFRelease(model.capturedValue);
        }
        model.capturedKey = static_cast<CFStringRef>(CFRetain(key));
        model.capturedDomain = static_cast<CFStringRef>(CFRetain(domain));
        model.capturedValue = CFRetain(value);
        CFDictionarySetValue(model.values, key, value);
    }

    static bool Sync(void* context, CFStringRef) {
        Model& model = *static_cast<Model*>(context);
        ++model.syncs;
        return model.syncSuccess;
    }

    PreferencesOps Operations() { return {this, Copy, Set, Sync}; }
};

#include "windows_string_vectors.h"

constexpr char FixtureDomain[] = "org.counterstrikeextended.fixture.settings.model.only";
constexpr char Key[] = "Software\\CounterStrikeExtendedFixture\\LongEnoughSettingName";

void RawBytesAndCanaries() {
    Model model;
    Store store(FixtureDomain, model.Operations());
    const std::array<unsigned char, 5> value{{0, 0x80, 0xff, 0xc0, 7}};
    Require(reinterpret_cast<std::uintptr_t>(value.data()) > UINT32_MAX, "source buffer not above 4GiB");
    Require(store.WriteBytes(Key, value.data(), value.size()), "raw write missing");
    std::array<unsigned char, 9> buffer;
    buffer.fill(0xa5);
    std::size_t required = 99;
    Require(store.ReadBytes(Key, buffer.data() + 2, 5, &required), "raw read failed");
    Require(required == value.size(), "required raw size differs");
    Require(std::equal(value.begin(), value.end(), buffer.begin() + 2), "raw payload changed");
    Require(buffer[0] == 0xa5 && buffer[1] == 0xa5 && buffer[7] == 0xa5 && buffer[8] == 0xa5,
        "caller buffer canary changed");
}

void StringNulAndEmptyData() {
    Model model;
    Store store(FixtureDomain, model.Operations());
    Require(store.WriteString(Key, "abc"), "string write failed");
    std::array<unsigned char, 6> bytes{{0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5}};
    std::size_t required = 0;
    Require(store.ReadBytes(Key, bytes.data(), bytes.size(), &required), "string read failed");
    Require(required == 4 && std::memcmp(bytes.data(), "abc\0", 4) == 0 && bytes[4] == 0xa5,
        "string NUL/capacity changed");
    Require(store.WriteString(Key, ""), "empty string failed");
    Require(store.ReadBytes(Key, nullptr, 0, &required) && required == 1, "empty string lacks NUL");
    Require(store.WriteBytes(Key, nullptr, 0), "zero-byte data failed");
    Require(store.ReadBytes(Key, nullptr, 0, &required) && required == 0, "zero-byte data is missing");
    Require(store.ReadBytes(Key, bytes.data(), 0, &required) && bytes[0] == 'a', "zero-byte read overwrote caller");
}

void IntegerEncodingAndRawCrossType() {
    Model model;
    Store store(FixtureDomain, model.Operations());
    for (const std::int32_t value : {INT32_MIN, -1, 0, INT32_MAX}) {
        Require(store.WriteInt32(Key, value), "integer write failed");
        std::array<unsigned char, 4> bytes{{}};
        std::size_t required = 0;
        Require(store.ReadBytes(Key, bytes.data(), bytes.size(), &required) && required == 4,
            "integer raw extent changed");
        const std::uint32_t bits = static_cast<std::uint32_t>(value);
        for (std::size_t index = 0; index < bytes.size(); ++index) {
            Require(bytes[index] == ((bits >> (index * CHAR_BIT)) & 0xffu), "integer bytes are not LE");
        }
        std::int32_t output = 17;
        Require(store.ReadInt32(Key, output) && output == value, "integer read changed bits");
    }
    Require(store.WriteString(Key, "A"), "short cross-type string failed");
    std::int32_t output = 0x12345678;
    Require(store.ReadInt32(Key, output) && output == 0x00000041, "short String staging residue differs");
    const std::array<UInt8, 2> shortBinary{{0x41, 0}};
    Require(store.WriteBytes(Key, shortBinary.data(), shortBinary.size()), "short Binary write failed");
    output = 0x12345678;
    Require(store.ReadInt32(Key, output) && output == 0x12340041, "short Binary suffix changed");
    Require(store.WriteBytes(Key, nullptr, 0), "zero-byte integer write failed");
    Require(store.ReadInt32(Key, output) && output == 0x12340041, "zero-byte integer changed caller");
    Require(store.WriteString(Key, "abcd"), "oversize cross-type string failed");
    Require(!store.ReadInt32(Key, output) && output == 0x12340041, "oversize integer changed caller");
}

void RequiredSizeAndUndersize() {
    Model model;
    Store store(FixtureDomain, model.Operations());
    Require(store.WriteString(Key, "four"), "write failed");
    std::size_t required = 99;
    Require(store.ReadBytes(Key, nullptr, 1, &required) && required == 5, "null destination size query failed");
    std::array<unsigned char, 4> value{{1, 2, 3, 4}};
    const auto before = value;
    Require(!store.ReadBytes(Key, value.data(), value.size(), &required), "undersize read accepted");
    Require(required == 5 && value == before, "undersize required/caller contract changed");
}

void MissingWrongTypesAndCopyFailure() {
    Model model;
    Store store(FixtureDomain, model.Operations());
    std::array<unsigned char, 4> value{{1, 2, 3, 4}};
    const auto before = value;
    std::size_t required = 99;
    Require(!store.ReadBytes(Key, value.data(), value.size(), &required) && required == 0 && value == before,
        "missing value modified caller");
    Require(store.WriteString(Key, "valid"), "write failed");
    const int number = 123;
    CFNumberRef wrong = CFNumberCreate(nullptr, kCFNumberIntType, &number);
    CFDictionarySetValue(model.values, model.capturedKey, wrong);
    CFRelease(wrong);
    Require(!store.ReadBytes(Key, value.data(), value.size(), &required) && required == 0 && value == before,
        "typed number coerced into raw data");
    CFStringRef text = CFStringCreateWithCString(nullptr, "not CFData", kCFStringEncodingUTF8);
    CFDictionarySetValue(model.values, model.capturedKey, text);
    CFRelease(text);
    Require(!store.ReadBytes(Key, value.data(), value.size(), &required) && required == 0 && value == before,
        "typed string coerced into raw data");
    Require(store.WriteString(Key, "valid"), "rewrite failed");
    model.copySuccess = false;
    Require(!store.ReadBytes(Key, value.data(), value.size(), &required) && required == 0 && value == before,
        "copy failure modified caller");
}

void SynchronizationFailures() {
    Model model;
    Store store(FixtureDomain, model.Operations());
    model.syncSuccess = false;
    Require(!store.WriteString(Key, "pending"), "failed synchronization acknowledged as write success");
    Require(model.sets == 1 && model.syncs == 1, "write did not set then synchronize");
    std::int32_t value = 0x12345678;
    const int copies = model.copies;
    Require(!store.ReadInt32(Key, value) && value == 0x12345678 && model.copies == copies,
        "failed read synchronization used stale cache");
}

void InvalidInputsAndLengthBounds() {
    Model model;
    Store store(FixtureDomain, model.Operations());
    const char invalid[] = {static_cast<char>(0xc0), static_cast<char>(0xaf), 0};
    for (const char* key : {static_cast<const char*>(nullptr), "", "Software\\", invalid}) {
        Require(!store.WriteString(key, "test"), "invalid key accepted");
    }
    Require(!store.WriteString(Key, nullptr), "null string accepted");
    Require(!store.WriteBytes(Key, nullptr, 1), "null nonempty bytes accepted");
    const std::size_t oversized = MaximumValueBytes + 1;
    Require(!store.WriteBytes(Key, reinterpret_cast<const void*>(1), oversized), "unrepresentable length accepted");
    std::size_t required = 99;
    Require(!store.ReadBytes(Key, reinterpret_cast<void*>(1), oversized, &required) && required == 0,
        "unrepresentable capacity accepted");
    Require(model.sets == 0 && model.copies == 0 && model.syncs == 0, "invalid input reached preferences");
}

void UnicodeKeyIdentity() {
    Model model;
    Store store(FixtureDomain, model.Operations());
    for (const auto& pair : std::vector<std::pair<const char*, const char*>>{
        {"Software\\MiXeD", "software\\mixed"}, {u8"S\\École", u8"s\\e\u0301COLE"},
        {u8"S\\Straße", "s\\STRASSE"}, {u8"S\\ПРИВЕТ", u8"s\\привет"},
        {u8"S\\Σς", u8"s\\σσ"}, {"S\\I", "s\\i"}}) {
        Require(store.WriteInt32(pair.first, 73), "Unicode write failed");
        std::int32_t output = 0;
        Require(store.ReadInt32(pair.second, output) && output == 73, "Unicode/ASCII case identity differs");
    }
    Require(store.WriteInt32("S\\a/b", 11) && store.WriteInt32("s\\a\\b", 22), "separator writes failed");
    std::int32_t output = 0;
    Require(store.ReadInt32("s\\a/b", output) && output == 11, "slash/backslash identity collapsed");
    Require(store.ReadInt32("s\\a\\b", output) && output == 22, "backslash identity changed");
    Require(store.WriteInt32(u8"S\\é", 5) && store.WriteInt32("S\\e", 6), "accent writes failed");
    Require(store.ReadInt32(u8"s\\É", output) && output == 5, "diacritic distinction lost");
}

void RetainedObjectsReleasedWithinCall() {
    Model model;
    Store store(FixtureDomain, model.Operations());
    Require(store.WriteString(Key, "nonempty reference counted data"), "write failed");
    Require(CFGetRetainCount(model.capturedKey) == 2, "key ownership escaped call");
    Require(CFGetRetainCount(model.capturedDomain) == 1, "domain ownership escaped call");
    Require(CFGetRetainCount(model.capturedValue) == 2, "data ownership escaped call");
    const CFIndex before = CFGetRetainCount(model.capturedValue);
    for (int index = 0; index < 50; ++index) {
        std::array<char, 64> buffer{{}};
        Require(store.ReadBytes(Key, buffer.data(), buffer.size()), "retained read failed");
    }
    Require(CFGetRetainCount(model.capturedValue) == before, "CopyValue return leaked");
}

void ActualWindowsUtf8StringCapacities() {
    Model model;
    Store store(FixtureDomain, model.Operations());
    for (const auto& test : StringQueries) {
        Require(store.WriteString(Key, StringInputs[test.input]), "reference vector write failed");
        std::array<unsigned char, 20> output;
        output.fill(0xa5);
        const auto unchanged = output;
        std::size_t required = 99;
        Require(store.ReadBytes(Key, output.data(), test.capacity, &required) == test.success,
            "UTF8-activeCodePage success/capacity differs");
        Require(required == test.required, "UTF8-activeCodePage required size differs");
        if (test.success) {
            Require(output == test.bytes, "UTF8-activeCodePage residue or caller canary differs");
        } else {
            // Windows defines ERROR_MORE_DATA contents as undefined; native failure leaves them intact.
            Require(output == unchanged, "undersize failure changed caller");
        }
    }
}

void InvalidUtf8StringDoesNotWrite() {
    Model model;
    Store store(FixtureDomain, model.Operations());
    const std::array<const char*, 5> invalid{{"\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80",
        "\xf0\x9f", "\x80"}};
    for (const char* value : invalid) {
        Require(!store.WriteString(Key, value), "invalid UTF8 String accepted");
        Require(model.sets == 0 && model.syncs == 0, "invalid String reached preferences mutation");
        Require(store.WriteBytes(Key, value, std::strlen(value)), "opaque Binary bytes rejected");
        model.sets = 0;
        model.syncs = 0;
    }
}

void TypedRecordAndMalformedMetadata() {
    Model model;
    Store store(FixtureDomain, model.Operations());
    const std::array<unsigned char, 2> bytes{{0x41, 0}};
    Require(store.WriteBytes(Key, bytes.data(), bytes.size()), "binary write failed");
    Require(CFGetTypeID(model.capturedValue) == CFDictionaryGetTypeID(), "missing typed single-value record");
    const ScopedCF<CFStringRef> versionKey(CreateUtf8String("version"));
    const ScopedCF<CFStringRef> kindKey(CreateUtf8String("kind"));
    const ScopedCF<CFStringRef> payloadKey(CreateUtf8String("payload"));
    const CFDictionaryRef record = static_cast<CFDictionaryRef>(model.capturedValue);
    Require(CFDictionaryGetCount(record) == 3, "record field count changed");
    int schemaVersion = 0;
    int binaryKind = -1;
    Require(CFNumberGetValue(static_cast<CFNumberRef>(CFDictionaryGetValue(record, versionKey.Get())),
        kCFNumberIntType, &schemaVersion) && schemaVersion == 1, "record version bytes changed");
    Require(CFNumberGetValue(static_cast<CFNumberRef>(CFDictionaryGetValue(record, kindKey.Get())),
        kCFNumberIntType, &binaryKind) && binaryKind == 0, "Binary provenance tag changed");
    const CFTypeRef payload = CFDictionaryGetValue(record, payloadKey.Get());
    Require(payload && CFGetTypeID(payload) == CFDataGetTypeID() &&
        CFDataGetLength(static_cast<CFDataRef>(payload)) == 2, "payload is not opaque CFData");
    const ScopedCF<CFPropertyListRef> binary(CFRetain(model.capturedValue));
    Require(store.WriteString(Key, "A"), "string write failed");
    Require(!CFEqual(binary.Get(), model.capturedValue), "identical bytes lost String/Binary provenance");
    int stringKind = -1;
    Require(CFNumberGetValue(static_cast<CFNumberRef>(CFDictionaryGetValue(
        static_cast<CFDictionaryRef>(model.capturedValue), kindKey.Get())), kCFNumberIntType, &stringKind) &&
        stringKind == 1, "String provenance tag changed");
    const auto original = static_cast<CFDictionaryRef>(model.capturedValue);
    const ScopedCF<CFDictionaryRef> saved(static_cast<CFDictionaryRef>(CFRetain(original)));
    std::array<unsigned char, 8> output;
    output.fill(0xa5);
    const auto unchanged = output;
    for (int mutation = 0; mutation < 8; ++mutation) {
        const ScopedCF<CFMutableDictionaryRef> corrupt(CFDictionaryCreateMutableCopy(nullptr, 0, saved.Get()));
        const int unknown = 999;
        const ScopedCF<CFNumberRef> integer(CFNumberCreate(nullptr, kCFNumberIntType, &unknown));
        const double floating = 1.0;
        const ScopedCF<CFNumberRef> number(CFNumberCreate(nullptr, kCFNumberDoubleType, &floating));
        if (mutation == 0) {
            CFDictionaryRemoveValue(corrupt.Get(), versionKey.Get());
        } else if (mutation == 1) {
            CFDictionarySetValue(corrupt.Get(), versionKey.Get(), integer.Get());
        } else if (mutation == 2) {
            CFDictionarySetValue(corrupt.Get(), versionKey.Get(), number.Get());
        } else if (mutation == 3) {
            CFDictionarySetValue(corrupt.Get(), kindKey.Get(), integer.Get());
        } else if (mutation == 4) {
            CFDictionarySetValue(corrupt.Get(), payloadKey.Get(), versionKey.Get());
        } else if (mutation == 5) {
            CFDictionarySetValue(corrupt.Get(), kindKey.Get(), kCFBooleanTrue);
        } else if (mutation == 6) {
            CFDictionarySetValue(corrupt.Get(), payloadKey.Get(), binary.Get());
        } else {
            CFDictionaryRemoveValue(corrupt.Get(), versionKey.Get());
            const ScopedCF<CFStringRef> unknownName(CreateUtf8String("unknown"));
            CFDictionarySetValue(corrupt.Get(), unknownName.Get(), integer.Get());
        }
        CFDictionarySetValue(model.values, model.capturedKey, corrupt.Get());
        std::size_t required = 99;
        Require(!store.ReadBytes(Key, output.data(), output.size(), &required) && required == 0 && output == unchanged,
            "malformed metadata accepted or caller modified");
    }
}

void MalformedStringPayloadNeverModifiesCaller() {
    Model model;
    Store store(FixtureDomain, model.Operations());
    Require(store.WriteString(Key, "valid"), "String setup failed");
    const ScopedCF<CFDictionaryRef> valid(static_cast<CFDictionaryRef>(CFRetain(model.capturedValue)));
    const ScopedCF<CFStringRef> payloadKey(CreateUtf8String("payload"));
    const std::vector<std::vector<UInt8>> malformed{{}, {0x41}, {0x41, 0, 0x42, 0},
        {0xc0, 0xaf, 0}, {0xed, 0xa0, 0x80, 0}, {0xf4, 0x90, 0x80, 0x80, 0}, {0xf0, 0x9f, 0}};
    for (const auto& bytes : malformed) {
        const ScopedCF<CFDataRef> payload(CFDataCreate(nullptr, bytes.data(), static_cast<CFIndex>(bytes.size())));
        const ScopedCF<CFMutableDictionaryRef> corrupt(CFDictionaryCreateMutableCopy(nullptr, 0, valid.Get()));
        CFDictionarySetValue(corrupt.Get(), payloadKey.Get(), payload.Get());
        CFDictionarySetValue(model.values, model.capturedKey, corrupt.Get());
        std::array<UInt8, 16> output;
        output.fill(0xa5);
        const auto unchanged = output;
        std::size_t required = 99;
        const int writesBefore = model.sets;
        Require(!store.ReadBytes(Key, output.data(), output.size(), &required) && required == 0 &&
            output == unchanged && model.sets == writesBefore, "malformed String data modified caller/store");
    }
}

void HighAddressGuardedCallerBuffer() {
    Model model;
    Store store(FixtureDomain, model.Operations());
    Require(store.WriteInt32(Key, -1), "write failed");
    const long pageResult = sysconf(_SC_PAGESIZE);
    Require(pageResult > 0, "page size unavailable");
    const std::size_t page = static_cast<std::size_t>(pageResult);
    void* mapping = mmap(nullptr, page * 3, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
    Require(mapping != MAP_FAILED, "guard mapping failed");
    unsigned char* middle = static_cast<unsigned char*>(mapping) + page;
    Require(mprotect(middle, page, PROT_READ | PROT_WRITE) == 0, "guard protection failed");
    unsigned char* target = middle + page - 4;
    Require(reinterpret_cast<std::uintptr_t>(target) > UINT32_MAX, "caller buffer not above 4GiB");
    Require(store.ReadBytes(Key, target, 4), "high-address read failed");
    Require(target[0] == 0xff && target[3] == 0xff, "high-address payload differs");
    Require(!store.ReadBytes(Key, target + 1, 3), "guarded undersize accepted");
    Require(store.WriteString(Key, "A"), "guarded String setup failed");
    constexpr std::size_t MaximumGuardedCapacity = 8;
    for (std::size_t capacity = 0; capacity <= MaximumGuardedCapacity; ++capacity) {
        std::memset(middle, 0xa5, page);
        target = middle + page - capacity;
        Require(store.ReadBytes(Key, target, capacity) == (capacity >= 2), "guarded String capacity differs");
        Require(target[-1] == 0xa5, "guarded String lower canary changed");
        if (capacity >= 2) {
            Require(target[0] == 0x41 && target[1] == 0, "guarded String bytes differ");
        }
        if (capacity >= 4) {
            Require(target[2] == 0 && target[3] == 0, "guarded String residue differs");
        }
    }
    Require(munmap(mapping, page * 3) == 0, "guard mapping cleanup failed");
}

} // namespace

int main() {
    const std::array<std::pair<const char*, void (*)()>, 14> tests{{
        {"raw bytes and canaries", RawBytesAndCanaries}, {"string NUL and empty data", StringNulAndEmptyData},
        {"integer encoding and cross-type", IntegerEncodingAndRawCrossType},
        {"required size and undersize", RequiredSizeAndUndersize},
        {"missing, wrong types and copy failure", MissingWrongTypesAndCopyFailure},
        {"synchronization failures", SynchronizationFailures}, {"invalid inputs and bounds", InvalidInputsAndLengthBounds},
        {"Unicode key identity", UnicodeKeyIdentity}, {"per-call CF ownership", RetainedObjectsReleasedWithinCall},
        {"typed record and malformed metadata", TypedRecordAndMalformedMetadata},
        {"actual Windows UTF8 String capacities", ActualWindowsUtf8StringCapacities},
        {"invalid UTF8 String does not write", InvalidUtf8StringDoesNotWrite},
        {"malformed String payload", MalformedStringPayloadNeverModifiesCaller},
        {"high-address guarded caller buffer", HighAddressGuardedCallerBuffer}
    }};
    int failures = 0;
    for (const auto& test : tests) {
        try {
            test.second();
            std::printf("PASS %s\n", test.first);
        } catch (const std::exception& error) {
            ++failures;
            std::printf("FAIL %s: %s\n", test.first, error.what());
        }
    }
    if (PersistentOperationAttempts != 0) {
        ++failures;
    }
    std::printf("cases=%zu failures=%d blocked_persistent_operation_attempts=%d windows_utf8_capacity_vectors=%zu\n",
        tests.size(), failures, PersistentOperationAttempts, sizeof(StringQueries) / sizeof(StringQueries[0]));
    return failures == 0 ? 0 : 1;
}
