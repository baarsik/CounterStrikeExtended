#include "VGUI_App.h"
#include "VGUI_SurfaceBase.h"
#include "VGui1AppPlatform.h"

#include <cstring>
#include <limits>
#include <memory>
#include <new>

namespace {
namespace platform = cse_vgui1_app;
constexpr std::uint64_t MaximumLegacyCount = (std::numeric_limits<int>::max)();

class Snapshot {
    const platform::ServicesV1& services_;

public:
    explicit Snapshot(const platform::ServicesV1& services) noexcept : services_(services), value{}, acquired(false) {
        acquired = services_.acquireClipboard(services_.context, &value);
    }
    ~Snapshot() {
        if (value.lease) {
            services_.releaseClipboard(services_.context, value.lease);
        }
    }
    bool Valid() const noexcept {
        return acquired && value.lease && value.storageBytes <= MaximumLegacyCount
            && (value.storageBytes == 0 || value.data);
    }
    platform::ClipboardSnapshot value;
    bool acquired;

};
}

namespace vgui {
void App::platTick() {
    platform::Call call("pump events");
    const auto& services = call.Services();
    const auto result = services.pumpEvents(services.context);
    if (result != platform::PumpResult::Empty && result != platform::PumpResult::Quit) {
        platform::RecordFailure(platform::Failure::BackendFailure);
        stop();
    }
}

void App::setCursorPos(int, int) {
    platform::Call call("set cursor position");
    // The retained Windows implementation intentionally does not warp.
}

void App::getCursorPos(int& x, int& y) {
    platform::Call call("get cursor position");
    x = 0;
    y = 0;
    if (_surfaceBaseDar.getCount() == 0 || !_surfaceBaseDar[0]) {
        platform::RecordFailure(platform::Failure::MissingSurface);
        return;
    }
    _surfaceBaseDar[0]->GetMousePos(x, y);
}

void App::internalSetMouseArena(int x0, int y0, int x1, int y1, bool enabled) {
    platform::Call call("set mouse arena");
    const auto& services = call.Services();
    if (!services.applyArena(services.context, x0, y0, x1, y1, enabled)) {
        platform::RecordFailure(platform::Failure::BackendFailure);
    }
}

long App::getTimeMillis() {
    std::uint32_t encoded = 0;
    if (!platform::ReadColdClock(encoded)) {
        platform::InvalidLegacyUse("cold clock", "clock read failed");
    }
    constexpr std::uint32_t MaximumSigned = (std::numeric_limits<std::int32_t>::max)();
    constexpr std::uint32_t MaximumEncoded = (std::numeric_limits<std::uint32_t>::max)();
    const std::int32_t signedMillis = encoded <= MaximumSigned ? static_cast<std::int32_t>(encoded)
        : -1 - static_cast<std::int32_t>(MaximumEncoded - encoded);
    return static_cast<long>(signedMillis);
}

void App::setClipboardText(const char* text, int textLen) {
    platform::Call call("set clipboard");
    if (!text || textLen <= 0 || static_cast<std::uint64_t>(textLen) >= MaximumLegacyCount) {
        platform::RecordFailure(platform::Failure::InvalidArgument);
        return;
    }
    const std::size_t bytes = static_cast<std::size_t>(textLen) + 1;
    std::unique_ptr<unsigned char[]> copy(new (std::nothrow) unsigned char[bytes]);
    if (!copy) {
        platform::RecordFailure(platform::Failure::AllocationFailure);
        return;
    }
    std::memcpy(copy.get(), text, static_cast<std::size_t>(textLen));
    copy[static_cast<std::size_t>(textLen)] = 0;
    const auto& services = call.Services();
    if (!services.writeClipboard(services.context, copy.get(), bytes)) {
        platform::RecordFailure(platform::Failure::BackendFailure);
    }
}

int App::getClipboardTextCount() {
    platform::Call call("clipboard count");
    Snapshot snapshot(call.Services());
    if (!snapshot.Valid()) {
        platform::RecordFailure(platform::Failure::InvalidSnapshot);
        return 0;
    }
    return static_cast<int>(snapshot.value.storageBytes);
}

int App::getClipboardText(int offset, char* destination, int capacity) {
    platform::Call call("read clipboard");
    if (offset < 0 || !destination || capacity <= 0) {
        platform::RecordFailure(platform::Failure::InvalidArgument);
        return 0;
    }
    Snapshot snapshot(call.Services());
    if (!snapshot.Valid()) {
        platform::RecordFailure(platform::Failure::InvalidSnapshot);
        return 0;
    }
    const std::uint64_t start = static_cast<std::uint64_t>(offset);
    if (start >= snapshot.value.storageBytes) {
        return 0;
    }
    const std::uint64_t remaining = snapshot.value.storageBytes - start;
    const std::uint64_t count = remaining < static_cast<std::uint64_t>(capacity)
        ? remaining : static_cast<std::uint64_t>(capacity);
    std::memcpy(destination, snapshot.value.data + static_cast<std::size_t>(start), static_cast<std::size_t>(count));
    return static_cast<int>(count);
}

bool App::setRegistryString(const char* key, const char* value) {
    platform::Call call("write setting string");
    if (!key || !value) {
        platform::RecordFailure(platform::Failure::InvalidArgument);
        return false;
    }
    const auto& services = call.Services();
    return services.writeString(services.context, key, value);
}

bool App::getRegistryString(const char* key, char* destination, int capacity) {
    platform::Call call("read setting bytes");
    if (!key || capacity < 0) {
        platform::RecordFailure(platform::Failure::InvalidArgument);
        return false;
    }
    const auto& services = call.Services();
    return services.readSetting(services.context, key, destination, static_cast<std::uint64_t>(capacity));
}

bool App::setRegistryInteger(const char* key, int value) {
    platform::Call call("write setting integer");
    if (!key) {
        platform::RecordFailure(platform::Failure::InvalidArgument);
        return false;
    }
    const auto& services = call.Services();
    return services.writeInteger(services.context, key, value);
}

bool App::getRegistryInteger(const char* key, int& value) {
    platform::Call call("read setting integer");
    if (!key) {
        platform::RecordFailure(platform::Failure::InvalidArgument);
        return false;
    }
    const auto& services = call.Services();
    return services.readSetting(services.context, key, &value, sizeof(value));
}

App* App::getInstance() {
    return _instance;
}
}
