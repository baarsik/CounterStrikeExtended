#include "VGui1MacFontProvider.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {
using namespace cse_vgui1;
void Require(bool value, const char* text) {
    if (!value) {
        std::fprintf(stderr, "FAILED injected provider: %s\n", text);
        std::abort();
    }
}
struct Services {
    std::vector<unsigned char> bytes;
    NativeFontLocation location;
    std::map<const void*, std::set<std::string>> owners;
    int lookups = 0;
    int reads = 0;
    int releases = 0;
    bool unavailable = false;
    bool failRead = false;
    bool failAcquire = false;
    bool failRelease = false;
    int lastWeight = -1;
    std::string lastIdentity;
};
bool Lookup(void* data, FT_LibraryRec_*, const FontRequest& request, NativeFontLocation& location) noexcept {
    auto& services = *static_cast<Services*>(data);
    ++services.lookups;
    services.lastIdentity = request.lookupIdentity;
    services.lastWeight = request.weight;
    if (services.unavailable) {
        return false;
    }
    location = services.location;
    return true;
}
bool Read(void* data, const char* path, std::vector<unsigned char>& bytes) noexcept {
    auto& services = *static_cast<Services*>(data);
    ++services.reads;
    Require(path == services.location.path, "resolved UTF8 path is not truncated/reinterpreted");
    if (services.failRead) {
        return false;
    }
    bytes = services.bytes;
    return true;
}
int Acquire(void* data, const void* owner, const char* path) noexcept {
    auto& services = *static_cast<Services*>(data);
    if (services.failAcquire) {
        return 0;
    }
    services.owners[owner].insert(path);
    return 1;
}
int Release(void* data, const void* owner) noexcept {
    auto& services = *static_cast<Services*>(data);
    ++services.releases;
    if (services.failRelease) {
        return 0;
    }
    services.owners.erase(owner);
    return 1;
}
std::vector<unsigned char> Input(const std::string& path) {
    FILE* file = nullptr;
#if defined(_MSC_VER)
    if (fopen_s(&file, path.c_str(), "rb") != 0) {
        file = nullptr;
    }
#else
    file = std::fopen(path.c_str(), "rb");
#endif
    Require(file != nullptr, "authored input");
    std::array<unsigned char, 32768> buffer{};
    const auto count = std::fread(buffer.data(), 1, buffer.size(), file);
    Require(count > 0 && count < buffer.size() && std::fclose(file) == 0, "authored input bounds");
    return {buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(count)};
}
FontRequest Request() {
    FontRequest request;
    request.requestedName = "CSE Owned Raster V1";
    request.lookupIdentity = request.requestedName;
    request.resolutionProfile = "exact-request-v1";
    request.tall = 20;
    request.weight = 400;
    return request;
}
}
int main(int argc, char** argv) {
    Require(argc == 2, "owned inputs argument");
    Services services;
    services.bytes = Input(std::string(argv[1]) + "/regular.ttf");
    services.location.path = "/owned/字体/" + std::string(300, 'p') + "/regular.ttf";
    services.location.postScriptName = "CSEOwnedRasterV1-Regular";
    MacFontServicesV1 callbacks{&services, Lookup, Read, Acquire, Release};
    const auto& provider = CseVgui1MacFontProviderCandidateV1();
    auto broken = callbacks;
    broken.Read = nullptr;
    Require(!CreateMacFontContextV1(broken), "incomplete services cannot create successful context");
    void* context = CreateMacFontContextV1(callbacks);
    Require(context != nullptr, "injected services context");
    int ownerA = 0;
    int ownerB = 0;
    Require(provider.RegisterFile(context, &ownerA, services.location.path.c_str()) == 0
        && provider.RegisterFile(context, &ownerA, services.location.path.c_str()) == 0
        && provider.RegisterFile(context, &ownerB, services.location.path.c_str()) == 0
        && services.owners.size() == 2, "two owners repeated registration retains full path");
    int error = 777;
    std::string diagnostic = "poison";
    std::vector<std::string> files{services.location.path};
    auto face = provider.Realize(context, Request(), files, error, diagnostic);
    Require(face && error == 0 && diagnostic.empty() && services.lastWeight == 400
        && services.lastIdentity == Request().lookupIdentity, "actual provider returns owned ready face");
    services.bytes.assign(services.bytes.size(), 0);
    int a = 0;
    int b = 0;
    int c = 0;
    Require(face->RawAbc('A', a, b, c) && b > 0, "face owns bytes independently of service buffers");
    services.failRelease = true;
    Require(provider.ReleaseOwner(context, &ownerA) != 0 && services.owners.size() == 2,
        "failed release retains both owner and context for explicit retry");
    provider.DestroyContext(context); // Must retain this context while tokens remain.
    services.failRelease = false;
    Require(provider.ReleaseOwner(context, &ownerA) == 0 && services.owners.size() == 1
        && services.owners.count(&ownerB) == 1, "successful retry never releases peer");
    services.unavailable = true;
    Require(!provider.Realize(context, Request(), files, error, diagnostic) && error != 0
        && !diagnostic.empty(), "ordinary unavailable exact identity");
    services.unavailable = false;
    services.failRead = true;
    Require(!provider.Realize(context, Request(), files, error, diagnostic) && error != 0,
        "read failure never reports a successful face");
    services.failRead = false;
    services.failAcquire = true;
    Require(provider.RegisterFile(context, &ownerA, services.location.path.c_str()) != 0,
        "native registration failure adapted from zero");
    const int releasesBeforeFailedToken = services.releases;
    Require(provider.ReleaseOwner(context, &ownerA) == 0
        && services.releases == releasesBeforeFailedToken, "failed acquire publishes no owned token");
    services.failAcquire = false;
    Require(provider.ReleaseOwner(context, &ownerA) == 0 && provider.ReleaseOwner(context, &ownerB) == 0,
        "all owned tokens released before context destruction");
    // A symbol request cannot use a same-named unbound installed program.
    services.bytes = Input(std::string(argv[1]) + "/symbol-high.ttf");
    services.location.postScriptName = "CSEOwnedSymbolV1-Regular";
    auto symbol = Request();
    symbol.requestedName = "Marlett";
    symbol.lookupIdentity = "Marlett";
    symbol.weight = 0;
    symbol.flags = FontSymbol;
    Require(!provider.Realize(context, symbol, {}, error, diagnostic) && error != 0,
        "unbound system Marlett cannot silently replace approved game resource");
    Require(provider.RegisterFile(context, &ownerA, services.location.path.c_str()) == 0,
        "explicit owned symbol resource registration");
    auto symbolFace = provider.Realize(context, symbol, files, error, diagnostic);
    Require(symbolFace && error == 0 && (symbolFace->Metrics().capabilities & CapabilitySymbol) != 0,
        "bound resource provides all critical widget byte mappings");
    symbolFace.reset();
    services.bytes = Input(std::string(argv[1]) + "/symbol-missing.ttf");
    Require(!provider.Realize(context, symbol, files, error, diagnostic) && error != 0
        && diagnostic.find("required widget byte") != std::string::npos,
        "missing critical symbol is unavailable rather than a successful .notdef checkbox");
    Require(provider.ReleaseOwner(context, &ownerA) == 0, "symbol registration releases normally");
    face.reset();
    provider.DestroyContext(context);
    Require(services.owners.empty(), "no retained fixture service registrations");
    std::puts("PASS injected provider ownership/failure; no native discovery/registration executed");
}
