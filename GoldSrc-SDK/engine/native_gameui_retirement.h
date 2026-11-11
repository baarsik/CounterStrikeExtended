#pragma once

#include "../public/native_gameui_retirement.h"

namespace cse {
bool TryRetireNativeGameUI();
bool TryShutdownNativeBaseUI();
bool TryShutdownNativeVGuiWrap1();
bool TryShutdownNativeVGuiWrap2();
bool TryShutdownNativeVGui();
bool TryShutdownNativeHost();
bool TryShutdownNativeGame();
bool TryUnloadNativeEngine();

enum class NativeHostInitPhase {
    NotStarted,
    CoreReady,
    ClientReady,
    VGuiStarted,
    VideoReady,
    DrawReady,
    RenderReady,
    SoundReady,
    CDReady,
    VoiceReady,
    DemoReady,
    ClientReadyForFrames,
    Complete
};
void SetNativeHostInitPhase(NativeHostInitPhase phase);
void ResetNativeHostShutdown();
struct NativeGameInitState {
    bool sysInitialized = false;
    bool memoryAllocated = false;
    bool launcherInitialized = false;
    bool modeAttempted = false;
    bool hostAttempted = false;
    bool authenticationInitialized = false;
    bool clientMouseActivated = false;
    bool steamClientInitialized = false;
    bool complete = false;
    bool shuttingDown = false;
};
extern NativeGameInitState nativeGameInit;
bool InitializeNativeGameUIHost();
bool NativeGameUIIsMainThread();
bool NativeGameUIIsRetirementSafePoint();
void EnterNativeGameUIUse();
void LeaveNativeGameUIUse();
bool EnterNativeGameUIRetirement();
void LeaveNativeGameUIRetirement();

class NativeGameUIUseScope final {
public:
    NativeGameUIUseScope() {
        EnterNativeGameUIUse();
    }
    ~NativeGameUIUseScope() {
        LeaveNativeGameUIUse();
    }
    NativeGameUIUseScope(const NativeGameUIUseScope &) = delete;
    NativeGameUIUseScope &operator=(const NativeGameUIUseScope &) = delete;
};
}

#if defined(__APPLE__)
#include "VGui1NativeRuntime.h"
namespace cse {
cse_vgui1_native::Result NativeVGui1RetirementResult() noexcept;
}
#endif
