#include "TextEntryClipboard.h"
#include "os_api.h"

#include <cassert>
#include <cstring>
#include <iostream>

namespace clipboard = cse_textentry_clipboard;
namespace {
bool Held(void*, const void* identity) {
    return identity == clipboard::NativePasteFrameIdentity();
}

clipboard::NativeFrameQuiescence Proof() {
    static int mainImageLifetime = 0;
    return {sizeof(clipboard::NativeFrameQuiescence), 1, &mainImageLifetime,
        clipboard::NativePasteFrameIdentity(), Held};
}

void ClosedCallsDoNotTouchOs() {
    const auto before = ReadKeyOsStats();
    int panel = 0;
    {
        clipboard::PasteFrame rejected(&panel, 1);
        assert(!rejected.Entered());
    }
    const auto after = ReadKeyOsStats();
    assert(before.creates == after.creates && before.gets == after.gets && before.sets == after.sets
        && before.locks == after.locks && before.unlocks == after.unlocks);
}
}

int main(int argc, char** argv) {
    assert(argc == 2);
    const char* mode = argv[1];
    int panel = 0;
    if (std::strcmp(mode, "unused") == 0) {
        clipboard::PasteFrame invalid(nullptr, 0);
        assert(!invalid.Entered() && ReadKeyOsStats().creates == 0);
        assert(clipboard::RetireNativePasteFrames(Proof()) == clipboard::NativeFrameRetireResult::Retired);
        assert(ReadKeyOsStats().deletes == 0);
        ClosedCallsDoNotTouchOs();
        return 0;
    }
    if (std::strcmp(mode, "zero") == 0) {
        SetZeroKeyMode(true);
    }
    if (std::strcmp(mode, "create-again") == 0 || std::strcmp(mode, "create-memory") == 0
        || std::strcmp(mode, "set-memory") == 0 || std::strcmp(mode, "lock") == 0) {
        const KeyFault fault = std::strcmp(mode, "create-again") == 0 ? KeyFault::CreateAgain
            : (std::strcmp(mode, "create-memory") == 0 ? KeyFault::CreateNoMemory
                : (std::strcmp(mode, "set-memory") == 0 ? KeyFault::SetNoMemory : KeyFault::LockBusy));
        SetKeyFault(fault);
        {
            clipboard::PasteFrame rejected(&panel, 2);
            assert(!rejected.Entered());
        }
        SetKeyFault(KeyFault::None);
    }
    if (std::strcmp(mode, "unlock-fatal") == 0) {
        SetKeyFault(KeyFault::UnlockPermission);
    }
    if (std::strcmp(mode, "destructor-lock-fatal") == 0) {
        SetKeyFault(KeyFault::DestructorLockBusy);
    }
    {
        clipboard::PasteFrame live(&panel, 2);
        assert(live.Entered());
        if (std::strcmp(mode, "restore-fatal") == 0) {
            SetKeyFault(KeyFault::RestoreNoMemory);
        }
    }
    const auto allocated = ReadKeyOsStats();
    assert(allocated.created == 1 && allocated.nonnullDestructors == 0 && allocated.keyOwned);
    if (std::strcmp(mode, "retire-lock") == 0) {
        SetKeyFault(KeyFault::LockBusy);
        assert(clipboard::RetireNativePasteFrames(Proof()) == clipboard::NativeFrameRetireResult::MutexLockFailed);
        assert(ReadKeyOsStats().keyOwned && ReadKeyOsStats().deleted == 0 && ReadKeyOsStats().destroyed == 0);
    } else if (std::strcmp(mode, "delete") == 0) {
        SetKeyFault(KeyFault::DeleteInvalid);
        assert(clipboard::RetireNativePasteFrames(Proof()) == clipboard::NativeFrameRetireResult::KeyDeleteFailed);
        assert(ReadKeyOsStats().keyOwned && ReadKeyOsStats().destroyed == 0);
        ClosedCallsDoNotTouchOs();
    } else if (std::strcmp(mode, "destroy") == 0) {
        SetKeyFault(KeyFault::DestroyBusy);
        assert(clipboard::RetireNativePasteFrames(Proof()) == clipboard::NativeFrameRetireResult::MutexDestroyFailed);
        assert(!ReadKeyOsStats().keyOwned && ReadKeyOsStats().deleted == 1);
        ClosedCallsDoNotTouchOs();
    }
    SetKeyFault(KeyFault::None);
    assert(clipboard::RetireNativePasteFrames(Proof()) == clipboard::NativeFrameRetireResult::Retired);
    const auto retired = ReadKeyOsStats();
    assert(!retired.keyOwned && retired.deleted == 1 && retired.destroyed == 1);
    ClosedCallsDoNotTouchOs();
    assert(clipboard::RetireNativePasteFrames(Proof()) == clipboard::NativeFrameRetireResult::Retired);
    std::cout << "PASS actual guard with controlled pthread boundary: " << mode << '\n';
}
