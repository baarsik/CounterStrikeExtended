#include "native/MacFontDiscovery.h"
#include "native/MacFontRegistration.h"

#include <CoreText/CoreText.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

constexpr const char* fixturePath = CSE_FONT_REGISTRATION_FIXTURE;
constexpr const char* fixtureFamily = u8"CSE 字体 Fixture with a long Unicode family name";

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

CFURLRef CreateUrl(const char* path) {
    return CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(path), std::strlen(path), false);
}

void CheckLookup(bool expected) {
    FT_Library library = nullptr;
    Require(FT_Init_FreeType(&library) == 0, "initialize pinned FreeType");
    FT_Int major = 0;
    FT_Int minor = 0;
    FT_Int patch = 0;
    FT_Library_Version(library, &major, &minor, &patch);
    Require(major == 2 && minor == 14 && patch == 3, "registration lookup uses pinned FreeType 2.14.3");
    cse::fonts::FontFile file;
    const bool found = cse::fonts::FindMacFontFile(library, fixtureFamily, 400, false, file);
    Require(found == expected, expected ? "registered family participates in lookup" : "released family leaves lookup");
    if (found) {
        Require(file.postScriptName == "CSEUnicodeFixture-Regular", "lookup selects the real Unicode fixture");
        FT_Face face = nullptr;
        Require(FT_New_Face(library, file.path.c_str(), file.faceIndex, &face) == 0, "open registered file and face");
        Require(FT_Get_Char_Index(face, 'A') != 0, "registered face has an actual glyph");
        FT_Done_Face(face);
    }
    FT_Done_FreeType(library);
}

void TestLifecycle() {
    int owner = 0;
    CheckLookup(false);
    Require(CseMacFontRegistrationAcquire(&owner, fixturePath), "register resolved UTF-8 font path");
    CheckLookup(true);
    Require(CseMacFontRegistrationAcquire(&owner, fixturePath), "same owner registration is idempotent");
    Require(CseMacFontRegistrationReleaseOwner(&owner), "final owner releases its registration");
    CheckLookup(false);
    Require(CseMacFontRegistrationReleaseOwner(&owner), "repeated owner release is idempotent");
}

void TestOwners() {
    int firstOwner = 0;
    int secondOwner = 0;
    int thirdOwner = 0;
    const std::filesystem::path alias = std::filesystem::path(fixturePath).parent_path() / u8"字体-font-alias.ttf";
    const std::filesystem::path hardLink = std::filesystem::path(fixturePath).parent_path() / u8"字体-hardlink.ttf";
    std::filesystem::remove(alias);
    std::filesystem::remove(hardLink);
    std::filesystem::create_symlink(fixturePath, alias);
    std::filesystem::create_hard_link(fixturePath, hardLink);
    Require(CseMacFontRegistrationAcquire(&firstOwner, fixturePath), "first owner registers font");
    Require(CseMacFontRegistrationAcquire(&secondOwner, alias.c_str()), "second owner joins through canonical alias");
    Require(CseMacFontRegistrationAcquire(&thirdOwner, hardLink.c_str()), "third owner joins through file identity");
    Require(CseMacFontRegistrationReleaseOwner(&firstOwner), "release first owner");
    CheckLookup(true);
    Require(CseMacFontRegistrationReleaseOwner(&secondOwner), "release second owner");
    CheckLookup(true);
    Require(CseMacFontRegistrationReleaseOwner(&thirdOwner), "last owner unregisters font");
    CheckLookup(false);
    std::filesystem::remove(alias);
    std::filesystem::remove(hardLink);
}

void TestExternal() {
    CFURLRef url = CreateUrl(fixturePath);
    Require(url && CTFontManagerRegisterFontsForURL(url, kCTFontManagerScopeProcess, nullptr),
        "external fixture registers process font");
    int owner = 0;
    int peer = 0;
    Require(CseMacFontRegistrationAcquire(&owner, fixturePath), "borrow exact pre-existing registration");
    Require(CseMacFontRegistrationAcquire(&peer, fixturePath), "second peer borrows pre-existing registration");
    const std::filesystem::path duplicate = std::string(fixturePath) + ".duplicate.ttf";
    std::filesystem::copy_file(fixturePath, duplicate, std::filesystem::copy_options::overwrite_existing);
    int duplicateOwner = 0;
    const bool duplicateRegistered = CseMacFontRegistrationAcquire(&duplicateOwner, duplicate.c_str()) != 0;
    Require(CseMacFontRegistrationReleaseOwner(&duplicateOwner), "release independent duplicate-name file ownership");
    CFURLRef duplicateUrl = CreateUrl(duplicate.c_str());
    Require(duplicateUrl && CTFontManagerGetScopeForURL(duplicateUrl) == kCTFontManagerScopeNone,
        "different-file registration is either rejected or owned and released, never incorrectly borrowed");
    CFRelease(duplicateUrl);
    std::printf("CoreText independent duplicate-name registration: %s\n",
        duplicateRegistered ? "accepted" : "rejected");
    CheckLookup(true);
    std::filesystem::remove(duplicate);
    Require(CseMacFontRegistrationReleaseOwner(&owner), "release first borrowed owner");
    Require(CseMacFontRegistrationReleaseOwner(&peer), "release final borrowed owner");
    CheckLookup(true);
    Require(CTFontManagerGetScopeForURL(url) == kCTFontManagerScopeProcess,
        "service leaves external process registration owned by the external registrar");
    Require(CTFontManagerUnregisterFontsForURL(url, kCTFontManagerScopeProcess, nullptr),
        "external registrar can release its own font");
    CFRelease(url);
    CheckLookup(false);
}

void TestFailure() {
    int owner = 0;
    Require(!CseMacFontRegistrationAcquire(nullptr, fixturePath), "null owner fails");
    Require(!CseMacFontRegistrationAcquire(&owner, nullptr), "null path fails");
    Require(!CseMacFontRegistrationAcquire(&owner, ""), "empty path fails");
    Require(!CseMacFontRegistrationAcquire(&owner, "/no-such-CSE-font-87921.ttf"), "missing file fails");
    const std::filesystem::path invalid = std::string(fixturePath) + ".invalid";
    {
        std::ofstream output(invalid);
        output << "This is not font data.";
    }
    Require(!CseMacFontRegistrationAcquire(&owner, invalid.c_str()), "invalid font data creates no registration");
    std::filesystem::remove(invalid);
    Require(CseMacFontRegistrationAcquire(&owner, fixturePath), "owner remains usable after failed registrations");
    const std::filesystem::path moved = std::string(fixturePath) + ".moved";
    std::filesystem::rename(fixturePath, moved);
    Require(!CseMacFontRegistrationReleaseOwner(&owner), "changed registered path fails release safely");
    std::filesystem::rename(moved, fixturePath);
    CheckLookup(true);
    Require(CseMacFontRegistrationReleaseOwner(&owner), "restored path allows retained owner release retry");
    CheckLookup(false);
}

void TestCollection() {
    int owner = 0;
    CheckLookup(false);
    const bool registered = CseMacFontRegistrationAcquire(&owner, CSE_FONT_REGISTRATION_COLLECTION) != 0;
    std::printf("CoreText mixed collection registration: %s\n", registered ? "accepted" : "rejected");
    if (registered) {
        CheckLookup(true);
        Require(CseMacFontRegistrationReleaseOwner(&owner), "release all owned faces in the registered collection");
    }
    CheckLookup(false);
}

struct FileSystemFixture {
    std::string fullPath;
    int calls = 0;
    bool fail = false;

    const char* GetLocalPath(const char*, char* output, int capacity) {
        ++calls;
        if (fail) {
            return nullptr;
        }
        std::strncpy(output, fullPath.c_str(), static_cast<std::size_t>(capacity));
        output[capacity - 1] = '\0';
        return output;
    }
};

FileSystemFixture* g_pFileSystem;

namespace vgui2 {
FileSystemFixture* filesystem() {
    return g_pFileSystem;
}
}

class BaseUISurface {
public:
    bool AddCustomFontFile(const char* filename);
};

class CWin32Surface {
public:
    bool AddCustomFontFile(const char* filename);
};

#include "surface_font_methods.h"

void TestSurfaces() {
    FileSystemFixture filesystem{fixturePath};
    g_pFileSystem = &filesystem;
    BaseUISurface engine;
    CWin32Surface vgui;
    Require(engine.AddCustomFontFile("virtual-engine-font.ttf"), "actual engine Surface method uses VFS registration");
    Require(vgui.AddCustomFontFile("virtual-vgui-font.ttf"), "actual VGUI Surface method uses VFS registration");
    Require(filesystem.calls == 2, "both actual Surface methods resolve their own requested path");
    Require(cse::fonts::ReleaseMacCustomFonts(&engine), "engine Surface owner detaches");
    CheckLookup(true);
    Require(cse::fonts::ReleaseMacCustomFonts(&vgui), "VGUI Surface owner detaches");
    CheckLookup(false);
    g_pFileSystem = nullptr;
    Require(!engine.AddCustomFontFile("missing.ttf"), "actual Surface method handles unavailable VFS");
}

void TestResolver() {
    int owner = 0;
    std::filesystem::path directory = std::filesystem::path(fixturePath).parent_path() / "long-path-fixture";
    const std::filesystem::path cleanupRoot = directory;
    constexpr int pathSegments = 10;
    for (int segment = 0; segment < pathSegments; ++segment) {
        directory /= u8"字体-font-path-segment-0123456789-0123456789";
    }
    std::filesystem::create_directories(directory);
    const std::filesystem::path longPath = directory / u8"完整字体.ttf";
    std::filesystem::copy_file(fixturePath, longPath, std::filesystem::copy_options::overwrite_existing);
    FileSystemFixture filesystem{longPath.string()};
    constexpr std::size_t legacyPathBytes = 260;
    Require(filesystem.fullPath.size() > legacyPathBytes, "fixture path exceeds the legacy path boundary");
    Require(cse::fonts::RegisterMacCustomFontFile(&owner, &filesystem, "virtual-font.ttf"),
        "resolve through the VFS interface and register the full UTF-8 path");
    Require(filesystem.calls > 1, "near-full VFS result triggers resolution with a larger buffer");
    CheckLookup(true);
    Require(cse::fonts::ReleaseMacCustomFonts(&owner), "release uses retained resolved path");
    CheckLookup(false);
    filesystem.fail = true;
    Require(!cse::fonts::RegisterMacCustomFontFile(&owner, &filesystem, "virtual-font.ttf"),
        "VFS resolution failure creates no font ownership");
    Require(cse::fonts::ReleaseMacCustomFonts(&owner), "release after VFS failure remains harmless");
    std::filesystem::remove_all(cleanupRoot);
}

void TestRestart() {
    int reusedOwnerToken = 0;
    constexpr int restarts = 64;
    for (int restart = 0; restart < restarts; ++restart) {
        Require(CseMacFontRegistrationAcquire(&reusedOwnerToken, fixturePath), "restart reuses owner token");
        CheckLookup(true);
        Require(CseMacFontRegistrationReleaseOwner(&reusedOwnerToken), "restart releases final owned registration");
        CheckLookup(false);
    }
}

void TestModules() {
    using Attach = int (*)(const char*);
    void* first = dlopen(CSE_FONT_OWNER_A, RTLD_NOW | RTLD_LOCAL);
    void* second = dlopen(CSE_FONT_OWNER_B, RTLD_NOW | RTLD_LOCAL);
    Require(first && second, "load independent consumer dylibs sharing one service");
    auto firstAttach = reinterpret_cast<Attach>(dlsym(first, "AttachFont"));
    auto secondAttach = reinterpret_cast<Attach>(dlsym(second, "AttachFont"));
    Require(firstAttach && secondAttach, "resolve both real consumer registration functions");
    Require(firstAttach(fixturePath) && secondAttach(fixturePath), "two independently loaded modules acquire the font");
    Require(dlclose(first) == 0, "unload first consumer without explicit shutdown");
    CheckLookup(true);
    Require(dlclose(second) == 0, "unload last consumer without explicit shutdown");
    CheckLookup(false);
    first = dlopen(CSE_FONT_OWNER_A, RTLD_NOW | RTLD_LOCAL);
    Require(first != nullptr, "reload previously detached consumer");
    firstAttach = reinterpret_cast<Attach>(dlsym(first, "AttachFont"));
    Require(firstAttach && firstAttach(fixturePath), "reloaded consumer token starts a fresh lifetime");
    Require(dlclose(first) == 0, "unload reloaded final consumer");
    CheckLookup(false);
}

void TestAtExit() {
    static int owner = 0;
    Require(std::atexit([] { CheckLookup(false); }) == 0, "register final lookup check before owner lifetime begins");
    struct ClientTeardownGuard {
        ~ClientTeardownGuard() {
            // This client is constructed before its first service call. Its
            // destructor runs after the real lease book has released the font.
            // Font absence alone cannot detect calls into a destroyed registry.
            if (!CseMacFontRegistrationReleaseOwner(&owner)) {
                std::fprintf(stderr, "FAIL: registration service was destroyed before client teardown completed\n");
                std::_Exit(EXIT_FAILURE);
            }
        }
    };
    static ClientTeardownGuard teardownGuard;
    FileSystemFixture filesystem{fixturePath};
    Require(cse::fonts::RegisterMacCustomFontFile(&owner, &filesystem, "virtual-font.ttf"),
        "register a client lease that must release during process teardown");
    CheckLookup(true);
}

}

int main(int argc, char** argv) {
    Require(argc == 2, "one font registration scenario is required");
    const std::string scenario = argv[1];
    if (scenario == "lifecycle") {
        TestLifecycle();
    } else if (scenario == "owners") {
        TestOwners();
    } else if (scenario == "external") {
        TestExternal();
    } else if (scenario == "failure") {
        TestFailure();
    } else if (scenario == "collection") {
        TestCollection();
    } else if (scenario == "resolver") {
        TestResolver();
    } else if (scenario == "surfaces") {
        TestSurfaces();
    } else if (scenario == "restart") {
        TestRestart();
    } else if (scenario == "modules") {
        TestModules();
    } else if (scenario == "exit") {
        TestAtExit();
    } else {
        Require(false, "unknown font registration scenario");
    }
    return EXIT_SUCCESS;
}
