#include "VGui1AppColdAdmission.h"
#include "fixture.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>

namespace admission = cse_vgui1_admission;
namespace {
void Check(bool condition, const char* description) {
    if (!condition) {
        const char* error = dlerror();
        std::fprintf(stderr, "FAIL: %s (%s)\n", description, error ? error : "no loader error");
        std::exit(1);
    }
}
bool Hex(const char* text, unsigned char* output, std::size_t count) {
    if (std::strlen(text) != count * 2) {
        return false;
    }
    for (std::size_t index = 0; index < count; ++index) {
        unsigned int value = 0;
        if (std::sscanf(text + index * 2, "%2x", &value) != 1) {
            return false;
        }
        output[index] = static_cast<unsigned char>(value);
    }
    return true;
}
admission::ImagePinV1 Pin(char** arguments, admission::metadata::Kind kind) {
    admission::ImagePinV1 pin{};
    char* end = nullptr;
    pin.fileBytes = std::strtoull(arguments[0], &end, 10);
    Check(end && !*end && pin.fileBytes > 0
        && Hex(arguments[1], pin.uuid, sizeof(pin.uuid))
        && Hex(arguments[2], pin.imageDigest.bytes, sizeof(pin.imageDigest.bytes))
        && Hex(arguments[3], pin.recordDigest.bytes, sizeof(pin.recordDigest.bytes)), "explicit external pins");
    pin.kind = kind;
    return pin;
}
bool Mapped(const char* path) {
    for (std::uint32_t index = 0; index < _dyld_image_count(); ++index) {
        char current[PATH_MAX]{};
        if (realpath(_dyld_get_image_name(index), current) && std::strcmp(current, path) == 0) {
            return true;
        }
    }
    return false;
}
void LoadedIdentity(void* address, const admission::ImageFacts& expected) {
    Dl_info info{};
    char current[PATH_MAX]{};
    Check(dladdr(address, &info) && realpath(info.dli_fname, current)
        && std::strcmp(current, expected.canonicalPath) == 0, "loaded symbol belongs to exact pinned image");
    const auto* header = static_cast<const mach_header_64*>(info.dli_fbase);
    Check(header && header->magic == MH_MAGIC_64 && header->ncmds < 4096
        && header->sizeofcmds < 1024 * 1024, "actual loaded bounded header");
    const auto* command = reinterpret_cast<const unsigned char*>(header + 1);
    std::size_t remaining = header->sizeofcmds;
    bool found = false;
    for (std::uint32_t index = 0; index < header->ncmds; ++index) {
        Check(remaining >= sizeof(load_command), "loaded command framing");
        const auto* item = reinterpret_cast<const load_command*>(command);
        Check(item->cmdsize >= sizeof(load_command) && item->cmdsize <= remaining, "loaded command bounds");
        if (item->cmd == LC_UUID) {
            Check(!found && item->cmdsize == sizeof(uuid_command), "loaded unique UUID");
            const auto* uuid = reinterpret_cast<const uuid_command*>(item);
            Check(std::memcmp(uuid->uuid, expected.uuid, sizeof(uuid->uuid)) == 0, "loaded UUID equals file pin");
            found = true;
        }
        command += item->cmdsize;
        remaining -= item->cmdsize;
    }
    Check(found && remaining == 0, "loaded UUID observed");
}
}

int main(int argc, char** argv) {
    Check(argc == 12, "mode and two externally pinned images required");
    const bool reject = std::strcmp(argv[1], "--reject") == 0;
    Check(reject || std::strcmp(argv[1], "--load") == 0, "known mode");
    const auto rootPin = Pin(argv + 3, admission::metadata::Kind::Requirement);
    const auto providerPin = Pin(argv + 8, admission::metadata::Kind::Offer);
    admission::ImageFacts root{};
    admission::ImageFacts provider{};
    Check(admission::ReadPinnedFile(argv[2], rootPin, root) == admission::Status::Ready
        && admission::ReadPinnedFile(argv[7], providerPin, provider) == admission::Status::Ready,
        "fresh exact file pins");
    Check(FixtureColdClockCalls() == 0 && FixtureProviderCalls() == 0
        && !Mapped(root.canonicalPath) && !Mapped(provider.canonicalPath),
        "no constructor or provider before admission");
    const auto status = admission::CheckCatalogPair(root, provider);
    if (reject) {
        Check(status != admission::Status::PinnedPairCompatible && FixtureColdClockCalls() == 0
            && FixtureProviderCalls() == 0 && !Mapped(root.canonicalPath),
            "mismatch rejected BEFORE dlopen/static constructors");
        std::printf("REJECTED before constructors: %s\n", admission::StatusName(status));
        return 0;
    }
    Check(status == admission::Status::PinnedPairCompatible,
        "catalog/absolute strong dependency admits controlled load");
    void* image = dlopen(root.canonicalPath, RTLD_NOW | RTLD_LOCAL);
    Check(image != nullptr, "controlled actual whole consumer load");
    using Observe = bool (*)(ColdObservation*);
    using Cleanup = bool (*)();
    auto observe = reinterpret_cast<Observe>(dlsym(image, "FixtureObserveRoot"));
    auto cleanup = reinterpret_cast<Cleanup>(dlsym(image, "FixtureCleanupRoot"));
    void* providerObserver = dlsym(image, "FixtureObserveProvider");
    Check(observe && cleanup && providerObserver, "typed fixture observations resolve");
    LoadedIdentity(reinterpret_cast<void*>(observe), root);
    LoadedIdentity(providerObserver, provider);
    ColdObservation observation{};
    Check(observe(&observation) && observation.appBytes == 664 && observation.fonts == 4
        && observation.cursors == 13 && observation.nextTick == 1234 && observation.minimumTick == 50
        && observation.coldClockCalls == 1 && FixtureProviderCalls() == 0, "actual complete cold constructor state");
    std::printf("COLD app=%u fonts=%u cursors=%u clock=%u tick=%u interval=%u provider_calls=%u\n",
        observation.appBytes, observation.fonts, observation.cursors, observation.coldClockCalls,
        observation.nextTick, observation.minimumTick, FixtureProviderCalls());
    Check(cleanup(), "fixture-only known-pristine nested cleanup and borrowed App pointer clear");
    Check(dlclose(image) == 0, "normal owned root release");
    Check(!Mapped(root.canonicalPath) && !Mapped(provider.canonicalPath), "actual independent root/provider unmapped");
    std::puts("PASS pinned matching actual cold constructors; services/font/Surface/UI readiness remains false");
    return 0;
}
