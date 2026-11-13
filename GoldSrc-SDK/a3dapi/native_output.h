#pragma once
#include "platform_types.h"
#include "native_platform.h"
#include <SDL3/SDL.h>
#include <array>
#include <atomic>
class A3dNativeMixer;
// Explicit dependency boundary for failure injection; defaults invoke the real SDL operations.
struct A3dNativeOutputOperations {
    bool (*init)(SDL_InitFlags);
    void (*quit)(SDL_InitFlags);
    SDL_AudioDeviceID* (*devices)(int*);
    SDL_AudioStream* (*open)(SDL_AudioDeviceID, const SDL_AudioSpec*, SDL_AudioStreamCallback, void*);
    void (*destroy)(SDL_AudioStream*);
    bool (*resume)(SDL_AudioStream*);
    bool (*put)(SDL_AudioStream*, const void*, int);
    SDL_Thread* (*thread)(SDL_ThreadFunction, const char*, void*);
    static A3dNativeOutputOperations Default() noexcept;
};
// Calls to Open/Start/Close/DeviceGuid are externally serialized. The mixer outlives Close.
class A3dNativeOutput {
public:
    explicit A3dNativeOutput(A3dNativeOutputOperations operations = A3dNativeOutputOperations::Default()) noexcept;
    ~A3dNativeOutput();
    A3dNativeOutput(const A3dNativeOutput&) = delete;
    A3dNativeOutput& operator=(const A3dNativeOutput&) = delete;
    HRESULT Open(const GUID* device, A3dNativeMixer& mixer);
    HRESULT Start();
    void Close() noexcept;
    HRESULT Status() const noexcept {
        return status_.load();
    }
    // Values are copied, process-local live-enumeration tokens; they are never persistent device identities.
    HRESULT DeviceGuid(DWORD index, GUID* guid);
private:
    static constexpr DWORD kChannels = 2;
    static constexpr DWORD kFrameBytes = kChannels * sizeof(SHORT);
    // DAL_A2D's 8192-byte ring and strict 4096-byte low-water mark determine
    // whole kernel invocations; SDL transport packet sizes must not split ramps.
    static constexpr DWORD kBufferBytes = 8192;
    static constexpr DWORD kLowWaterBytes = 4096;
    static constexpr DWORD kTransportBytes = 256 * kFrameBytes;
    static constexpr Uint32 kPollMilliseconds = 20;
    using Prepared = std::array<SHORT, kBufferBytes / sizeof(SHORT)>;
    static int Produce(void* context);
    static void SDLCALL Consume(void* context, SDL_AudioStream* stream, int additional, int total);
    A3dNativeOwnerLease lifetime_{A3dNativeOwnerKind::Output};
    A3dNativeOutputOperations operations_;
    SDL_AudioStream* stream_ = nullptr;
    SDL_Mutex* mutex_ = nullptr;
    SDL_Thread* producer_ = nullptr;
    A3dNativeMixer* mixer_ = nullptr;
    Prepared queue_{};
    DWORD read_ = 0;
    DWORD write_ = 0;
    DWORD queuedBytes_ = 0;
    Uint64 retryAfter_ = 0;
    bool audioOwned_ = false;
    bool started_ = false;
    std::atomic<bool> stopping_{true};
    std::atomic<HRESULT> status_{S_OK};
};
