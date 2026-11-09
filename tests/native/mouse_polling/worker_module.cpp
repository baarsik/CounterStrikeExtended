#include "native_mouse_poller_win.h"
#include <atomic>
#include <type_traits>

static std::atomic<unsigned int> reads{0};
static cse::mouse::CallResult Cursor(void *, cse::mouse::Point &point, cse::mouse::SampleReason reason) {
    if (reason == cse::mouse::SampleReason::Poll) {
        ++reads;
    }
    point = cse::mouse::Point{};
    return {true, 0};
}
static cse::mouse::WinBackend ownedBackend(cse::mouse::WinBackend::KernelServices(Cursor, nullptr));
static cse::mouse::Poller<cse::mouse::WinBackend> poller(ownedBackend);
static_assert(std::is_trivially_destructible<decltype(poller)>::value, "Explicit lifetime, no detach cleanup");
extern "C" __declspec(dllexport) bool CseStart() {
    return poller.Start() && poller.Publish(true, 1);
}
extern "C" __declspec(dllexport) unsigned int CseReads() {
    return reads.load();
}
extern "C" __declspec(dllexport) bool CseStop() {
    return poller.Stop();
}
BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) {
    return TRUE;
}
