#include "native_mouse_window.h"
#include <cstdint>

namespace {
using Observer = void (*)(std::uint32_t);
Observer observer = nullptr;
std::uint32_t epoch = 0;
struct Marker {
    ~Marker() {
        if (observer) {
            observer(epoch);
        }
    }
};
Marker marker;
} // namespace

extern "C" __attribute__((visibility("default"))) std::uint32_t
CseFixture_AttachConsumer(std::uint32_t requestedEpoch, Observer callback) {
    if (!requestedEpoch || !callback || epoch) {
        return 0;
    }
    epoch = requestedEpoch;
    observer = callback;
    return 1;
}

extern "C" __attribute__((visibility("default"))) std::uint32_t CseFixture_BindWindow(SDL_Window *window) {
    return cse::NativeMouseWindowState().Bind(window) ? 1U : 0U;
}

extern "C" __attribute__((visibility("default"))) void CseFixture_CloseConsumer() {
    cse::NativeMouseWindowState().Invalidate(nullptr);
}
