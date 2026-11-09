#include "TextEntryClipboard.h"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
#include <memory>

#if !defined(__APPLE__) || !defined(CSE_TEXT_ENTRY_USE_PTHREAD_KEY) || CSE_TEXT_ENTRY_USE_PTHREAD_KEY != 1
#error This fixture must explicitly enable the native Apple candidate
#endif

namespace clipboard = cse_textentry_clipboard;

namespace {
struct HostProof {
    const void* identity;
    bool held = false;
    int checks = 0;
    static bool IsHeld(void* context, const void* identity) {
        auto& proof = *static_cast<HostProof*>(context);
        ++proof.checks;
        return proof.held && proof.identity == identity;
    }
    clipboard::NativeFrameQuiescence Value() {
        return {sizeof(clipboard::NativeFrameQuiescence), 1, this, identity, IsHeld};
    }
};

void BehaviorAndRetirement() {
    HostProof proof{clipboard::NativePasteFrameIdentity()};
    auto descriptor = proof.Value();
    assert(clipboard::RetireNativePasteFrames(descriptor) == clipboard::NativeFrameRetireResult::QuiescenceRequired);
    int panel = 0;
    {
        clipboard::PasteFrame outer(&panel, 5);
        clipboard::PasteFrame equal(&panel, 5);
        clipboard::PasteFrame larger(&panel, 6);
        assert(outer.Entered() && !equal.Entered() && !larger.Entered());
        {
            clipboard::PasteFrame smaller(&panel, 4);
            clipboard::PasteFrame same(&panel, 4);
            assert(smaller.Entered() && !same.Entered());
        }
        // Deliberately dishonest witness: internal frame consistency must still
        // refuse retirement. This is NOT an example of genuine host quiescence.
        proof.held = true;
        assert(clipboard::RetireNativePasteFrames(descriptor) == clipboard::NativeFrameRetireResult::Busy);
        proof.held = false;
        bool separateThread = false;
        std::thread worker([&] {
            clipboard::PasteFrame frame(&panel, 5);
            separateThread = frame.Entered();
        });
        worker.join();
        assert(separateThread);
        bool caught = false;
        try {
            clipboard::PasteFrame progress(&panel, 3);
            assert(progress.Entered());
            throw std::runtime_error("controlled unwind");
        } catch (const std::runtime_error&) {
            caught = true;
        }
        assert(caught);
        clipboard::PasteFrame retry(&panel, 3);
        assert(retry.Entered());
    }
    {
        std::vector<int> panels(2048);
        std::vector<std::unique_ptr<clipboard::PasteFrame>> frames;
        for (int& identity : panels) {
            frames.emplace_back(new clipboard::PasteFrame(&identity, 1));
            assert(frames.back()->Entered());
        }
        while (!frames.empty()) {
            frames.pop_back();
        }
    }
    {
        clipboard::PasteFrame invalid(nullptr, 0);
        clipboard::PasteFrame negative(&panel, -1);
        assert(!invalid.Entered() && !negative.Entered());
    }
    descriptor.moduleIdentity = &panel;
    const int before = proof.checks;
    assert(clipboard::RetireNativePasteFrames(descriptor) == clipboard::NativeFrameRetireResult::InvalidProof);
    assert(proof.checks == before);
    descriptor = proof.Value();
    proof.held = true; // Main-image fixture, all calls/objects/workers now drained.
    assert(clipboard::RetireNativePasteFrames(descriptor) == clipboard::NativeFrameRetireResult::Retired);
    {
        clipboard::PasteFrame closed(&panel, 5);
        assert(!closed.Entered()); // Must not touch deleted key/destroyed mutex.
    }
    assert(clipboard::RetireNativePasteFrames(descriptor) == clipboard::NativeFrameRetireResult::Retired);
}
}

int main() {
    BehaviorAndRetirement();
    std::cout << "PASS candidate progress/thread/unwind/depth/proof/retirement controls\n";
}
