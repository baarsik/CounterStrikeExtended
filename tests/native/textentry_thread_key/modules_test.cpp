#include "module_api.h"

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstring>
#include <dlfcn.h>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace clipboard = cse_textentry_clipboard;
namespace {
bool CounterOnly = false;
bool SkipRetirement = false;

struct Image {
    void* initial = nullptr;
    void* retained = nullptr;
    const KeyFrameModuleApi* api = nullptr;
    const void* dataAddress = nullptr;
    const void* identity = nullptr;
    std::atomic<int> calls{0};
    std::atomic<int> objects{0};
    bool admissionClosed = false;
    bool exclusiveGate = false;
    bool workersQuiescent = true;
    bool lifecycleCall = false;
    bool retired = false;
    int destructors = 0;

    Image(const char* path, const char* name) {
        initial = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
        assert(initial);
        api = static_cast<const KeyFrameModuleApi*>(dlsym(initial, name));
        assert(api);
        dataAddress = api;
        identity = api->identity();
        api->setNotice(Notice, this);
        if (!CounterOnly) {
            retained = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
            assert(retained);
        }
    }

    static void Notice(void* context, int tag) {
        auto& self = *static_cast<Image*>(context);
        assert(self.calls.load() == 0 && self.objects.load() == 0 && self.workersQuiescent && !self.lifecycleCall
            && "real code reference released before complete calls/objects drained");
        assert(self.retired && "key retirement was skipped before unload");
        ++self.destructors;
        std::cout << "destructor_tag=" << tag << '\n';
    }

    static bool IsHeld(void* context, const void* identity) {
        auto& self = *static_cast<Image*>(context);
        return self.retained && self.identity == identity && self.admissionClosed && self.exclusiveGate
            && self.calls.load() == 0 && self.objects.load() == 0 && self.workersQuiescent;
    }

    clipboard::NativeFrameQuiescence Proof() {
        return {sizeof(clipboard::NativeFrameQuiescence), 1, this, identity, IsHeld};
    }

    void DropInitial() {
        assert(dlclose(initial) == 0);
        initial = nullptr;
        assert(destructors == 0);
    }

    void* Begin(const void* panel, int remainder, bool rejectionProbe = false) {
        assert(!admissionClosed || rejectionProbe);
        ++calls;
        void* frame = api->begin(panel, remainder);
        ++objects;
        --calls;
        return frame;
    }

    bool Entered(void* frame) {
        ++calls;
        const bool entered = api->entered(frame);
        --calls;
        return entered;
    }

    void End(void* frame) {
        ++calls;
        api->end(frame);
        --objects;
        --calls;
    }

    clipboard::NativeFrameRetireResult Retire() {
        assert(!lifecycleCall);
        lifecycleCall = true;
        const auto result = api->retire(Proof());
        lifecycleCall = false;
        if (result == clipboard::NativeFrameRetireResult::Retired) {
            retired = true;
        }
        return result;
    }

    void ClosedProbe(const void* panel) {
        const auto before = api->stats();
        void* frame = Begin(panel, 1, true);
        assert(!Entered(frame));
        // Entered-frame count is0, but this rejected object still needs module
        // destructor code. The genuine host proof therefore refuses retirement.
        assert(Retire() == clipboard::NativeFrameRetireResult::QuiescenceRequired);
        End(frame);
        const auto after = api->stats();
        assert(before.creates == after.creates && before.gets == after.gets && before.sets == after.sets
            && before.locks == after.locks && before.unlocks == after.unlocks);
    }

    void Drain() {
        assert(retained && calls.load() == 0 && objects.load() == 0 && !lifecycleCall);
        assert(retired || SkipRetirement);
        api = nullptr;
        void* image = retained;
        retained = nullptr;
        assert(dlclose(image) == 0);
        Dl_info information{};
        const bool mapped = dladdr(dataAddress, &information) != 0;
        assert(destructors == 1 && !mapped);
        std::cout << "after_host_drain: destructors=" << destructors << " mapped=" << mapped << '\n';
    }
};

struct Park {
    std::mutex mutex;
    std::condition_variable changed;
    bool inReader = false;
    bool releaseReader = false;
    bool parked = false;
    bool exit = false;
};
Park* CurrentPark = nullptr;

int BlockedReader(wchar_t* destination, int bytes) {
    assert(bytes == 4 * static_cast<int>(sizeof(wchar_t)));
    auto& park = *CurrentPark;
    std::unique_lock<std::mutex> lock(park.mutex);
    park.inReader = true;
    park.changed.notify_all();
    park.changed.wait(lock, [&] { return park.releaseReader; });
    destination[0] = L'Q';
    destination[1] = 0;
    return 2;
}

void Run(const char* firstPath, const char* secondPath, bool zero) {
    Image first(firstPath, "cse_key_frame_api_a");
    Image second(secondPath, "cse_key_frame_api_b");
    assert(first.identity != second.identity);
    first.api->setZeroKey(zero);
    second.api->setZeroKey(zero);
    int panel = 0;
    void* a = first.Begin(&panel, 5);
    void* b = second.Begin(&panel, 5);
    assert(first.Entered(a) && second.Entered(b));
    first.DropInitial();
    second.DropInitial();
    assert(first.Retire() == clipboard::NativeFrameRetireResult::QuiescenceRequired);
    void* repeat = first.Begin(&panel, 5);
    void* smaller = first.Begin(&panel, 4);
    assert(!first.Entered(repeat) && first.Entered(smaller));
    first.End(smaller);
    first.End(repeat);
    first.End(a);

    Park park;
    CurrentPark = &park;
    first.workersQuiescent = false;
    std::thread worker([&] {
        void* frame = first.Begin(&panel, 5);
        assert(first.Entered(frame));
        first.End(frame);
        ++first.calls;
        assert(first.api->readBuffer(BlockedReader)); // Real Buffer outside any PasteFrame.
        --first.calls;
        std::unique_lock<std::mutex> lock(park.mutex);
        park.parked = true;
        park.changed.notify_all();
        park.changed.wait(lock, [&] { return park.exit; });
        // Main-image-only continuation: no module table, frame or callback use.
    });
    {
        std::unique_lock<std::mutex> lock(park.mutex);
        park.changed.wait(lock, [&] { return park.inReader; });
        first.admissionClosed = true;
        first.exclusiveGate = true;
        assert(first.objects.load() == 0 && first.calls.load() == 1);
        assert(first.Retire() == clipboard::NativeFrameRetireResult::QuiescenceRequired);
        park.releaseReader = true;
        park.changed.notify_all();
        park.changed.wait(lock, [&] { return park.parked; });
    }
    assert(first.calls.load() == 0 && first.objects.load() == 0);
    first.workersQuiescent = true; // Observed parked after its final module return, not inferred from counter0.
    if (!SkipRetirement) {
        first.api->setFault(KeyFault::DeleteInvalid);
        assert(first.Retire() == clipboard::NativeFrameRetireResult::KeyDeleteFailed);
        assert(first.api->stats().keyOwned);
        first.ClosedProbe(&panel);
        first.api->setFault(KeyFault::DestroyBusy);
        assert(first.Retire() == clipboard::NativeFrameRetireResult::MutexDestroyFailed);
        assert(!first.api->stats().keyOwned && first.api->stats().deleted == 1);
        first.ClosedProbe(&panel);
        first.api->setFault(KeyFault::None);
        assert(first.Retire() == clipboard::NativeFrameRetireResult::Retired);
        first.ClosedProbe(&panel);
        const auto state = first.api->stats();
        assert(state.created == 1 && state.deleted == 1 && state.destroyed == 1 && state.nonnullDestructors == 0);
    }
    first.Drain();

    // Thread stayed alive outside module code through actual unmap. A null TSD
    // destructor cannot jump back into the removed image when this worker exits.
    {
        std::lock_guard<std::mutex> lock(park.mutex);
        park.exit = true;
        park.changed.notify_all();
    }
    worker.join();
    assert(second.Entered(b));
    void* progress = second.Begin(&panel, 4);
    assert(second.Entered(progress));
    second.End(progress);
    second.End(b);
    second.admissionClosed = true;
    second.exclusiveGate = true;
    assert(second.Retire() == clipboard::NativeFrameRetireResult::Retired);
    second.ClosedProbe(&panel);
    second.Drain();
}
}

int main(int argc, char** argv) {
    assert(argc == 3 || argc == 4);
    CounterOnly = argc == 4 && std::strcmp(argv[3], "counter-only") == 0;
    SkipRetirement = argc == 4 && std::strcmp(argv[3], "skip-retirement") == 0;
    const int rounds = argc == 4 && std::strcmp(argv[3], "single-lifecycle") == 0 ? 1 : 4;
    for (int round = 0; round < rounds; ++round) {
        Run(argv[1], argv[2], round % 2 != 0);
    }
    std::cout << "PASS real guard/Buffer calls, closed admission, partial retirement, worker exit, key reuse and unmap\n";
}
