// This global owns cleanup implemented in another translation unit and has no SDK header dependency.
#if defined(_WIN32)
extern "C" void CseSteamFixtureLateCleanup();
#else
extern "C" __attribute__((visibility("hidden"))) void CseSteamFixtureLateCleanup();
#endif

namespace {
struct LateGlobalOwner {
    ~LateGlobalOwner() {
        CseSteamFixtureLateCleanup();
    }
};
LateGlobalOwner lateGlobalOwner;
} // namespace
