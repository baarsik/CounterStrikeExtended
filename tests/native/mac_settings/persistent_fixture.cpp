// Deliberately excluded from CTest. This executable requires a unique owned domain
// and explicit mode; no static initialization performs preferences operations.
#include "../../../GoldSrc-SDK/common/mac_settings_store.cpp"

#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>

namespace {
using namespace cse::mac_settings;
constexpr char FixturePrefix[] = "org.counterstrikeextended.fixture.settings.";
constexpr int WorkerKeys = 64;

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool OwnedDomain(const std::string& domain) {
    const std::string prefix(FixturePrefix);
    if (domain.size() != prefix.size() + 32 || domain.compare(0, prefix.size(), prefix) != 0) {
        return false;
    }
    for (std::size_t index = prefix.size(); index < domain.size(); ++index) {
        const char value = domain[index];
        if (!((value >= '0' && value <= '9') || (value >= 'a' && value <= 'f'))) {
            return false;
        }
    }
    return true;
}

void WriteInitial(const Store& store, const char* domainName) {
    const std::array<unsigned char, 5> raw{{0, 0x80, 0xff, 0xc0, 7}};
    Require(store.WriteString("Software\\CSEFixture\\Text", u8"native UTF8: Ж 🙂"), "string write failed");
    Require(store.WriteInt32("Software\\CSEFixture\\Integer", INT32_MIN), "integer write failed");
    Require(store.WriteBytes("Software\\CSEFixture\\Raw", raw.data(), raw.size()), "raw write failed");
    Require(store.WriteString("Software\\CSEFixture\\Short", "A"), "short string write failed");
    Require(store.WriteBytes("Software\\CSEFixture\\Empty", nullptr, 0), "empty write failed");
    Require(store.WriteString("Software\\CSEFixture\\EmptyString", ""), "empty String write failed");
    Require(store.WriteInt32(u8"Software\\CSEFixture\\École", 73), "Unicode key write failed");
    const ScopedCF<CFStringRef> domain(CreateUtf8String(domainName));
    const ScopedCF<CFMutableStringRef> key(CreateKey("Software\\CSEFixture\\WrongType"));
    const ScopedCF<CFStringRef> wrong(CreateUtf8String("owned malformed typed value"));
    Require(domain.Get() && key.Get() && wrong.Get(), "typed value construction failed");
    CFPreferencesSetValue(key.Get(), wrong.Get(), domain.Get(), kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    Require(CFPreferencesSynchronize(domain.Get(), kCFPreferencesCurrentUser, kCFPreferencesAnyHost),
        "typed fixture synchronization failed");
}

void ReadInitial(const Store& store) {
    std::array<char, 128> bytes{{}};
    std::size_t required = 0;
    const char* expected = u8"native UTF8: Ж 🙂";
    Require(store.ReadBytes("software\\csefixture\\TEXT", bytes.data(), bytes.size(), &required) &&
        required == std::strlen(expected) + 1 && std::memcmp(bytes.data(), expected, required) == 0,
        "restart text/case bytes differ");
    std::int32_t value = 0;
    Require(store.ReadInt32("Software\\CSEFixture\\Integer", value) && value == INT32_MIN, "restart integer differs");
    const std::array<unsigned char, 5> raw{{0, 0x80, 0xff, 0xc0, 7}};
    Require(store.ReadBytes("Software\\CSEFixture\\Raw", bytes.data(), bytes.size(), &required) &&
        required == raw.size() && std::memcmp(bytes.data(), raw.data(), raw.size()) == 0, "restart raw bytes differ");
    value = 0x12345678;
    Require(store.ReadInt32("Software\\CSEFixture\\Short", value) && value == 0x00000041,
        "restart short String residue differs");
    value = 0x12345678;
    Require(store.ReadInt32("Software\\CSEFixture\\EmptyString", value) && value == 0x12340000,
        "restart empty String residue differs");
    Require(store.ReadBytes("Software\\CSEFixture\\Empty", nullptr, 0, &required) && required == 0,
        "restart empty value missing");
    Require(store.ReadInt32(u8"software\\csefixture\\e\u0301COLE", value) && value == 73,
        "restart Unicode key equivalence differs");
    value = 0x12345678;
    Require(!store.ReadInt32("Software\\CSEFixture\\WrongType", value) && value == 0x12345678,
        "stored non-CFData value accepted");
}

void Worker(const Store& store, const std::string& label) {
    for (int index = 0; index < WorkerKeys; ++index) {
        const std::string key = "Software\\CSEFixture\\Concurrent\\" + label + "\\" + std::to_string(index);
        Require(store.WriteInt32(key.c_str(), index + (label == "left" ? 1000 : 2000)),
            "concurrent key write failed");
    }
}

void ReadWorkers(const Store& store) {
    for (const char* label : {"left", "right"}) {
        for (int index = 0; index < WorkerKeys; ++index) {
            const std::string key = "Software\\CSEFixture\\Concurrent\\" + std::string(label) + "\\" +
                std::to_string(index);
            std::int32_t value = 0;
            Require(store.ReadInt32(key.c_str(), value) && value == index + (std::string(label) == "left" ? 1000 : 2000),
                "concurrent keys were lost or changed");
        }
    }
    ReadInitial(store);
}

CFIndex RemoveOwnedKeys(const char* domainName) {
    const ScopedCF<CFStringRef> domain(CreateUtf8String(domainName));
    Require(domain.Get() && CFPreferencesSynchronize(domain.Get(), kCFPreferencesCurrentUser, kCFPreferencesAnyHost),
        "cleanup initial synchronization failed");
    const ScopedCF<CFArrayRef> keys(CFPreferencesCopyKeyList(domain.Get(),
        kCFPreferencesCurrentUser, kCFPreferencesAnyHost));
    const CFIndex count = keys.Get() ? CFArrayGetCount(keys.Get()) : 0;
    for (CFIndex index = 0; index < count; ++index) {
        const CFTypeRef key = CFArrayGetValueAtIndex(keys.Get(), index);
        Require(CFGetTypeID(key) == CFStringGetTypeID(), "owned domain contains non-string key");
        CFPreferencesSetValue(static_cast<CFStringRef>(key), nullptr, domain.Get(),
            kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    }
    Require(CFPreferencesSynchronize(domain.Get(), kCFPreferencesCurrentUser, kCFPreferencesAnyHost),
        "cleanup synchronization failed");
    return count;
}

void VerifyEmpty(const char* domainName) {
    const ScopedCF<CFStringRef> domain(CreateUtf8String(domainName));
    Require(domain.Get() && CFPreferencesSynchronize(domain.Get(), kCFPreferencesCurrentUser, kCFPreferencesAnyHost),
        "empty-domain synchronization failed");
    const ScopedCF<CFArrayRef> keys(CFPreferencesCopyKeyList(domain.Get(),
        kCFPreferencesCurrentUser, kCFPreferencesAnyHost));
    Require(!keys.Get() || CFArrayGetCount(keys.Get()) == 0, "owned preference keys remain");
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 4 || std::string(argv[1]) != "--owned-domain" || !OwnedDomain(argv[2])) {
        std::fputs("A unique owned fixture domain and reviewed operation are required.\n", stderr);
        return 2;
    }
    const std::string operation(argv[3]);
    if (operation != "write" && operation != "read" && operation != "worker-left" && operation != "worker-right" &&
        operation != "read-workers" && operation != "cleanup" && operation != "verify-empty") {
        return 2;
    }
    try {
        const Store store(argv[2], NativeOperations());
        if (operation == "write") {
            WriteInitial(store, argv[2]);
        } else if (operation == "read") {
            ReadInitial(store);
        } else if (operation == "worker-left" || operation == "worker-right") {
            Worker(store, operation.substr(7));
        } else if (operation == "read-workers") {
            ReadWorkers(store);
        } else if (operation == "cleanup") {
            const CFIndex count = RemoveOwnedKeys(argv[2]);
            std::printf("removed_owned_keys=%ld\n", static_cast<long>(count));
        } else {
            VerifyEmpty(argv[2]);
        }
        std::printf("PASS operation=%s\n", operation.c_str());
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL operation=%s reason=%s\n", operation.c_str(), error.what());
        return 1;
    }
}
