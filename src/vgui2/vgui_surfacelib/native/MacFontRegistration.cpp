#include "MacFontRegistration.h"

#include <CoreText/CoreText.h>

#include <cstdlib>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <sys/stat.h>

namespace {

template <typename T> class OwnedCF {
public:
    explicit OwnedCF(T value = nullptr) : value_(value) {}
    ~OwnedCF() {
        if (value_) {
            CFRelease(value_);
        }
    }
    OwnedCF(const OwnedCF&) = delete;
    OwnedCF& operator=(const OwnedCF&) = delete;
    T get() const {
        return value_;
    }

private:
    T value_;
};

struct FileIdentity {
    dev_t device;
    ino_t inode;

    bool operator<(const FileIdentity& other) const {
        return device < other.device || (device == other.device && inode < other.inode);
    }
};

bool SameFile(const char* path, const FileIdentity& identity) {
    struct stat metadata = {};
    return stat(path, &metadata) == 0 && metadata.st_dev == identity.device && metadata.st_ino == identity.inode;
}

CFURLRef CreateUrl(const std::string& path) {
    return CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(path.c_str()), static_cast<CFIndex>(path.size()), false);
}

bool HasError(CFErrorRef error, CFIndex code) {
    return error && CFEqual(CFErrorGetDomain(error), kCTFontManagerErrorDomain) && CFErrorGetCode(error) == code;
}

struct Registration {
    std::string path;
    std::set<const void*> owners;
    bool owned = false;
};

bool EnsureRegistered(Registration& registration) {
    OwnedCF<CFURLRef> url(CreateUrl(registration.path));
    if (!url.get()) {
        return false;
    }
    if (CTFontManagerGetScopeForURL(url.get()) != kCTFontManagerScopeNone) {
        // Already present at this exact URL: the external registrar owns it.
        return true;
    }
    CFErrorRef registrationError = nullptr;
    const bool registered = CTFontManagerRegisterFontsForURL(url.get(), kCTFontManagerScopeProcess, &registrationError);
    OwnedCF<CFErrorRef> error(registrationError);
    if (registered) {
        registration.owned = true;
        return true;
    }
    // A concurrent external registrar may have registered this exact file.
    // A duplicate PostScript name from another file does not confer ownership.
    return HasError(error.get(), kCTFontManagerErrorAlreadyRegistered)
        && CTFontManagerGetScopeForURL(url.get()) != kCTFontManagerScopeNone;
}

bool Unregister(const FileIdentity& identity, const Registration& registration) {
    if (!registration.owned) {
        return true;
    }
    if (!SameFile(registration.path.c_str(), identity)) {
        std::fprintf(stderr, "macOS custom-font release retained ownership: registered file moved or changed: %s\n",
            registration.path.c_str());
        return false;
    }
    OwnedCF<CFURLRef> url(CreateUrl(registration.path));
    if (!url.get()) {
        return false;
    }
    if (CTFontManagerGetScopeForURL(url.get()) == kCTFontManagerScopeNone) {
        return true;
    }
    CFErrorRef releaseError = nullptr;
    const bool removed = CTFontManagerUnregisterFontsForURL(url.get(), kCTFontManagerScopeProcess, &releaseError);
    OwnedCF<CFErrorRef> error(releaseError);
    if (removed || HasError(error.get(), kCTFontManagerErrorNotRegistered)) {
        return true;
    }
    std::fprintf(stderr, "macOS custom-font release failed (%ld): %s\n",
        error.get() ? CFErrorGetCode(error.get()) : 0L, registration.path.c_str());
    return false;
}

class Registry {
public:
    ~Registry() {
        for (const auto& entry : registrations_) {
            if (!Unregister(entry.first, entry.second)) {
                std::fprintf(stderr,
                    "Unable to clean up an owned macOS font while unloading the registration service\n");
            }
        }
    }

    bool Acquire(const void* owner, const char* path) {
        if (!owner || !path || !path[0]) {
            return false;
        }
        OwnedCF<CFStringRef> utf8(CFStringCreateWithCString(kCFAllocatorDefault, path, kCFStringEncodingUTF8));
        if (!utf8.get()) {
            return false;
        }
        std::unique_ptr<char, decltype(&std::free)> canonical(realpath(path, nullptr), &std::free);
        if (!canonical) {
            return false;
        }
        struct stat metadata = {};
        if (stat(canonical.get(), &metadata) != 0 || !S_ISREG(metadata.st_mode)) {
            return false;
        }
        const FileIdentity identity{metadata.st_dev, metadata.st_ino};
        std::lock_guard<std::mutex> lock(mutex_);
        auto existing = registrations_.find(identity);
        if (existing != registrations_.end()) {
            if (!SameFile(existing->second.path.c_str(), identity)) {
                return false;
            }
            const auto inserted = existing->second.owners.insert(owner);
            if (!EnsureRegistered(existing->second)) {
                if (inserted.second) {
                    existing->second.owners.erase(inserted.first);
                }
                return false;
            }
            return true;
        }
        // Allocate bookkeeping before CoreText gains a process registration, so
        // allocation failure cannot leave an untracked catalog entry behind.
        Registration registration;
        registration.path = canonical.get();
        registration.owners.insert(owner);
        const auto inserted = registrations_.emplace(identity, std::move(registration));
        if (!EnsureRegistered(inserted.first->second)) {
            registrations_.erase(inserted.first);
            return false;
        }
        return true;
    }

    bool ReleaseOwner(const void* owner) {
        if (!owner) {
            return false;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        bool released = true;
        for (auto entry = registrations_.begin(); entry != registrations_.end();) {
            const auto found = entry->second.owners.find(owner);
            if (found == entry->second.owners.end()) {
                ++entry;
                continue;
            }
            if (entry->second.owners.size() > 1) {
                entry->second.owners.erase(found);
                ++entry;
                continue;
            }
            if (!Unregister(entry->first, entry->second)) {
                // Preserve the final owner and resolved path for an explicit
                // retry; never report success after dropping responsibility.
                released = false;
                ++entry;
                continue;
            }
            entry = registrations_.erase(entry);
        }
        return released;
    }

private:
    std::mutex mutex_;
    std::map<FileIdentity, Registration> registrations_;
};

Registry& ProcessRegistry() {
    static Registry registry;
    return registry;
}

class RegistryModuleInitializer {
public:
    RegistryModuleInitializer() {
        // Register service destruction at dylib load, before any dependent
        // client's lease book can register its own teardown callback.
        static_cast<void>(ProcessRegistry());
    }
};

const RegistryModuleInitializer registryModuleInitializer;

}

extern "C" int CseMacFontRegistrationAcquire(const void* owner, const char* path) noexcept {
    try {
        return ProcessRegistry().Acquire(owner, path);
    } catch (...) {
        std::fprintf(stderr, "Unable to allocate macOS custom-font registration ownership\n");
        return 0;
    }
}

extern "C" int CseMacFontRegistrationReleaseOwner(const void* owner) noexcept {
    try {
        return ProcessRegistry().ReleaseOwner(owner);
    } catch (...) {
        std::fprintf(stderr, "Unable to release macOS custom-font registration ownership\n");
        return 0;
    }
}
