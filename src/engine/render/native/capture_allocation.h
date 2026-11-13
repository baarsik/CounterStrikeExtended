#pragma once

namespace cse {
namespace render {

// Private ownership checkpoints; never part of a client or backend allocator API.
enum class CaptureAllocationSite {
    EventGrowthAfterDraw,
    PendingListStorage,
    ListUploadCopy,
    TextureImageStorage,
    TextureObjectVersion,
    DrawSnapshotCopy,
    RecorderConstruction
};

#if defined(CSE_CAPTURE_FAULT_INJECTION) && CSE_CAPTURE_FAULT_INJECTION == 1
using CaptureAllocationHook = void (*)(CaptureAllocationSite);

inline CaptureAllocationHook& ActiveCaptureAllocationHook() {
    static thread_local CaptureAllocationHook hook = nullptr;
    return hook;
}

inline void BeforeCaptureAllocation(CaptureAllocationSite site) {
    if (const auto hook = ActiveCaptureAllocationHook()) {
        hook(site);
    }
}
#else
inline void BeforeCaptureAllocation(CaptureAllocationSite) {
}
#endif

}
}
