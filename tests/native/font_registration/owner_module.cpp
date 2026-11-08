#include "native/MacFontRegistration.h"

#include <string>

// Match the production case: both modules instantiate the helper for the same
// publicly named filesystem type, with ordinary default symbol visibility.
struct FontFixtureFileSystem {
    std::string path;

    const char* GetLocalPath(const char*, char* buffer, int size) {
        std::snprintf(buffer, static_cast<std::size_t>(size), "%s", path.c_str());
        return buffer;
    }
};

namespace {

int ownerToken;

}

extern "C" __attribute__((visibility("default"))) int AttachFont(const char* path) {
    FontFixtureFileSystem filesystem{path};
    return cse::fonts::RegisterMacCustomFontFile(&ownerToken, &filesystem, "fixture.ttf");
}

extern "C" __attribute__((visibility("default"))) int ReleaseFonts() {
    return cse::fonts::ReleaseMacCustomFonts(&ownerToken);
}
