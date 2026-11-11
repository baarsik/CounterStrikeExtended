#include "native_vgui_completion.h"
#include <cassert>
#include <cstdio>
#include <pthread.h>
#include <thread>

struct Counter {
    int count = 0;
    int CountValue() const {
        return count;
    }
    int Count() const {
        return count;
    }
};
namespace vgui2 {
void* g_pSurface = reinterpret_cast<void*>(1);
void* g_pFileSystem = reinterpret_cast<void*>(2);
void* g_pIPanel = reinterpret_cast<void*>(3);
void* g_pLocalize = reinterpret_cast<void*>(4);
} // namespace vgui2
void* g_pKeyValues = reinterpret_cast<void*>(5);
class CVGui {
public:
    cse::vgui_completion::Result FinishNativeClient(const void*) noexcept;
    void Shutdown();
    bool m_nativeSealed = false;
    bool m_nativeOwnerSet = true;
    pthread_t m_nativeOwner = pthread_self();
    unsigned int m_nativeFrameDepth = 0;
    bool m_InDispatcher = false;
    bool m_bInitialized = true;
    bool m_bRunning = true;
    Counter m_PanelList;
    Counter m_TickSignalVec;
    Counter m_MessageQueue;
    Counter m_SecondaryQueue;
    Counter m_DelayedMessageQueue;
    Counter m_Contexts;
    void DestroyAllContexts() {
        m_Contexts.count = 0;
    }
    void ClearMessageQueues() {
        m_MessageQueue.count = m_SecondaryQueue.count = m_DelayedMessageQueue.count = 0;
    }
};
#include "actual_vgui_completion.inc"
int main() {
    CVGui owner;
#ifdef CSE_COMPLETION_BASELINE
    owner.Shutdown();
    if (vgui2::g_pSurface || vgui2::g_pFileSystem || vgui2::g_pIPanel || vgui2::g_pLocalize || g_pKeyValues) {
        std::fputs("FAIL actual CVGui Shutdown retains borrowed engine/filesystem views\n", stderr);
        return 1;
    }
#else
    using cse::vgui_completion::Result;
    const void* surface = vgui2::g_pSurface;
    assert(owner.FinishNativeClient(nullptr) == Result::ForeignOwner);
    std::thread foreign([&]() { assert(owner.FinishNativeClient(surface) == Result::Busy); });
    foreign.join();
    owner.m_nativeFrameDepth = 1;
    assert(owner.FinishNativeClient(surface) == Result::Busy);
    owner.m_nativeFrameDepth = 0;
    owner.m_InDispatcher = true;
    assert(owner.FinishNativeClient(surface) == Result::Busy);
    owner.m_InDispatcher = false;
    for (auto* pending : {&owner.m_PanelList, &owner.m_TickSignalVec, &owner.m_MessageQueue, &owner.m_SecondaryQueue,
                          &owner.m_DelayedMessageQueue}) {
        pending->count = 1;
        assert(owner.FinishNativeClient(surface) == Result::Busy && vgui2::g_pSurface == surface);
        pending->count = 0;
    }
    owner.m_Contexts.count = 2;
    assert(owner.FinishNativeClient(surface) == Result::Complete);
    assert(!vgui2::g_pSurface && !vgui2::g_pFileSystem && !vgui2::g_pIPanel && !vgui2::g_pLocalize && !g_pKeyValues);
    assert(owner.m_nativeSealed && !owner.m_bRunning && !owner.m_bInitialized && !owner.m_Contexts.count);
    assert(owner.FinishNativeClient(surface) == Result::Complete);
    std::puts(
        "PASS actual provider finish rejects live owners/callbacks, preserves views on failure and seals on retry");
#endif
}
