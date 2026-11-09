#include "module_fixture.h"
#include "NativeClipboardMac.h"
#include <cstdlib>
#include <vector>

namespace owner = cse_vgui2_clipboard_owner;
namespace core = cse_vgui2_clipboard;

namespace {
void (*Notice)(void*, int) = nullptr;
void* NoticeContext = nullptr;

__attribute__((destructor)) void Unloaded() {
    if (Notice) {
        Notice(NoticeContext, FIXTURE_TAG);
    }
}

struct Component {
    FixtureHost host{};
    owner::Owner clipboard;
    owner::Snapshot snapshot{};
    std::vector<unsigned char> bundle;

    static bool Thread(void*) { return true; }
    static bool Pin(void* context, std::uint64_t* token) {
        auto& self = *static_cast<Component*>(context);
        return self.host.pin(self.host.context, token);
    }
    static bool Defer(void* context, std::uint64_t token) {
        auto& self = *static_cast<Component*>(context);
        return self.host.defer(self.host.context, token);
    }
    static void* Allocate(void*, std::size_t size) { return std::malloc(size); }
    static void Free(void*, void* data) { std::free(data); }
    static void Diagnostic(void* context, const char*, core::ByteSpan name) {
        auto& self = *static_cast<Component*>(context);
        self.host.named(self.host.context, name.data, name.storageBytes);
    }
    static owner::Status Open(void*) { return owner::Status::Ok; }
    static owner::Status Read(void* context, owner::HostRead* output) {
        auto& self = *static_cast<Component*>(context);
        auto* data = new std::vector<unsigned char>(self.bundle);
        *output = {{data->data(), data->size()}, data, 1, 1};
        return owner::Status::Ok;
    }
    static bool Release(void* context, void* token) {
        auto& self = *static_cast<Component*>(context);
        if (!self.host.retireAllowed(self.host.context)) {
            return false;
        }
        delete static_cast<std::vector<unsigned char>*>(token);
        return true;
    }
    static owner::Status Stage(void*, core::ByteSpan, core::ByteSpan, bool) { return owner::Status::Ok; }
    static owner::Status Prepare(void*, std::int64_t* count) {
        *count = 1;
        return owner::Status::Ok;
    }
    static owner::Status Types(void*, bool, std::int64_t*) { return owner::Status::Ok; }
    static owner::Status Set(void* context, core::ByteSpan bytes, std::int64_t) {
        auto& self = *static_cast<Component*>(context);
        self.bundle.assign(bytes.data, bytes.data + bytes.storageBytes);
        return owner::Status::Ok;
    }
    static owner::Status Close(void*) { return owner::Status::Ok; }
};
}

#define FIXTURE_EXPORT extern "C" __attribute__((visibility("default")))
FIXTURE_EXPORT int FixtureTag() { return FIXTURE_TAG; }
FIXTURE_EXPORT int FixtureCreate(const FixtureHost* host, void** output) {
    auto* self = new Component;
    self->host = *host;
    Notice = host->unloaded;
    NoticeContext = host->context;
    const owner::DependenciesV1 dependencies{sizeof(owner::DependenciesV1), 1, self, host->epoch,
        Component::Thread, Component::Pin, Component::Defer, Component::Allocate, Component::Free, nullptr,
        Component::Diagnostic};
#if defined(FIXTURE_REAL_MAC)
    const auto result = owner::CreateNamedFixtureOwner(dependencies, self->clipboard);
#else
    const owner::BackendV1 backend{sizeof(owner::BackendV1), 1, self, Component::Open, Component::Read,
        Component::Release, Component::Stage, Component::Prepare, Component::Types, Component::Set,
        Component::Set, Component::Close};
    const auto result = self->clipboard.Open(dependencies, backend);
#endif
    *output = self;
    return static_cast<int>(result);
}
FIXTURE_EXPORT int FixturePublish(void* context) {
    auto& self = *static_cast<Component*>(context);
    const unsigned char bytes[] = {'L', 0};
    const unsigned char units[] = {'L', 0, 0, 0};
    return static_cast<int>(self.clipboard.Publish({true, {bytes, 2}, 101, true, {units, 4}, 4, false, 0}));
}
FIXTURE_EXPORT int FixtureBorrow(void* context) {
    auto& self = *static_cast<Component*>(context);
    return static_cast<int>(self.clipboard.Acquire(self.snapshot));
}
FIXTURE_EXPORT int FixtureReadCell(void* context) {
    auto& self = *static_cast<Component*>(context);
    wchar_t cell = 0;
    std::int32_t count = 0;
    const auto status = core::ReadUnicodeCells(self.snapshot.unicode, 0, {&cell, sizeof(cell)}, count);
    return status == core::Status::Ok && count == 1 ? static_cast<int>(cell) : -1;
}
FIXTURE_EXPORT int FixtureEnd(void* context) {
    auto& self = *static_cast<Component*>(context);
    return static_cast<int>(self.clipboard.EndBorrow(self.snapshot));
}
FIXTURE_EXPORT int FixtureRetry(void* context) {
    auto& self = *static_cast<Component*>(context);
    return static_cast<int>(self.clipboard.RetryRetirements());
}
FIXTURE_EXPORT int FixtureCloseDestroy(void* context) {
    auto* self = static_cast<Component*>(context);
    const auto result = self->clipboard.Close();
    if (result == owner::Status::Ok) {
        delete self;
    }
    return static_cast<int>(result);
}
