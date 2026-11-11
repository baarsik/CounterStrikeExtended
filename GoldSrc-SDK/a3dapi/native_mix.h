#pragma once
#include "platform_dsound.h"
#include "a2d.h"
#include "HrtfMgr.h"
#include "A3dSource.h"

class A3dNativeBuffer;

// Mutable buffer/list/kernel work shares one lock because the retained kernel
// also owns process-wide scratch. No foreign interface calls occur under it.
class A3dNativeMixLock {
public:
    A3dNativeMixLock() noexcept;
    ~A3dNativeMixLock();
    A3dNativeMixLock(const A3dNativeMixLock&) = delete;
    A3dNativeMixLock& operator=(const A3dNativeMixLock&) = delete;
};

class A3dNativeMixer {
public:
    A3dNativeMixer() = default;
    ~A3dNativeMixer();
    A3dNativeMixer(const A3dNativeMixer&) = delete;
    A3dNativeMixer& operator=(const A3dNativeMixer&) = delete;
    void Shutdown() noexcept;
    DWORD BufferCount() const;
    HRESULT Render(SHORT* stereo, DWORD frames);
    HRESULT CreateBuffer(LPCDSBUFFERDESC description, IDirectSoundBuffer** buffer);
    HRESULT DuplicateBuffer(IDirectSoundBuffer* source, IDirectSoundBuffer** buffer);
private:
    friend class A3dNativeBuffer;
    A3dNativeBuffer* first_ = nullptr;
    A3dNativeBuffer* last_ = nullptr;
};

class A3dNativeVoice {
public:
    A3dNativeVoice() = default;
    A3dNativeVoice(const A3dNativeVoice&) = delete;
    A3dNativeVoice& operator=(const A3dNativeVoice&) = delete;
    HRESULT SetControl(const A3DCTRL_SRC_SUPER& control);
private:
    friend class A3dNativeBuffer;
    friend class A3dNativeMixer;
    HRESULT Initialize(BYTE* samples, DWORD bytes, const WAVEFORMATEX& format);
    HRESULT ApplyControl(const A3DCTRL_SRC_SUPER& control);
    bool CanRender(DWORD frames) const;
    a3d_mix_input_t input_{};
    a3d_mix_voice_t voice_{};
    A3DCTRL_SRC_SUPER control_{};
    CHrtfMgr hrtf_;
    DWORD frequency_ = 0;
    bool initialized_ = false;
};
