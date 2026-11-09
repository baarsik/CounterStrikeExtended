// Controlled reference only: whole owned legacy classes, real Windows SDK types,
// and explicit fake multimedia calls. No recording device or provider is opened.
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include <initguid.h>
#include <dsound.h>
#include <crtdbg.h>

#include <algorithm>
#include <array>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <vector>

#if defined(NDEBUG)
extern "C" _ACRTIMP void __cdecl _wassert(const wchar_t*, const wchar_t*, unsigned int);
#endif

namespace {

constexpr DWORD captureRate = 11025;
constexpr unsigned int waveBufferCount = 15;
constexpr unsigned int waveBufferBytes = 256;
constexpr unsigned int sampleBytes = sizeof(short);
constexpr unsigned int defaultCallerSampleCapacity = 1024;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "legacy voice reference: %s\n", message);
        std::exit(1);
    }
}

struct WaveState {
    using Callback = void (CALLBACK*)(HWAVEIN, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
    Callback callback = nullptr;
    DWORD_PTR instance = 0;
    WAVEFORMATEX format = {};
    std::array<WAVEHDR*, waveBufferCount> headers = {};
    std::array<bool, waveBufferCount> queued = {};
    unsigned int prepared = 0;
    unsigned int starts = 0;
    unsigned int closes = 0;
    HWAVEIN handle = nullptr;
};

WaveState wave;

MMRESULT WINAPI FixtureWaveOpen(HWAVEIN* handle, UINT device, const WAVEFORMATEX* format,
    DWORD_PTR callback, DWORD_PTR instance, DWORD flags) {
    Require(device == WAVE_MAPPER && flags == CALLBACK_FUNCTION, "actual WaveIn default-device callback contract");
    wave = {};
    wave.format = *format;
    wave.callback = reinterpret_cast<WaveState::Callback>(callback);
    wave.instance = instance;
    wave.handle = reinterpret_cast<HWAVEIN>(&wave);
    *handle = wave.handle;
    return MMSYSERR_NOERROR;
}

MMRESULT WINAPI FixtureWavePrepare(HWAVEIN handle, WAVEHDR* header, UINT bytes) {
    Require(handle == wave.handle && bytes == sizeof(WAVEHDR) && wave.prepared < wave.headers.size(),
        "actual WaveIn header preparation");
    wave.headers[wave.prepared++] = header;
    header->dwFlags = WHDR_PREPARED;
    return MMSYSERR_NOERROR;
}

MMRESULT WINAPI FixtureWaveAdd(HWAVEIN handle, WAVEHDR* header, UINT bytes) {
    Require(handle == wave.handle && bytes == sizeof(WAVEHDR), "actual WaveIn buffer recycling");
    for (unsigned int index = 0; index < wave.headers.size(); ++index) {
        if (wave.headers[index] == header) {
            Require(!wave.queued[index], "header is not submitted twice");
            wave.queued[index] = true;
            header->dwFlags = WHDR_PREPARED;
            return MMSYSERR_NOERROR;
        }
    }
    return MMSYSERR_INVALPARAM;
}

MMRESULT WINAPI FixtureWaveStart(HWAVEIN handle) {
    Require(handle == wave.handle, "WaveIn starts its owned device");
    ++wave.starts;
    return MMSYSERR_NOERROR;
}

MMRESULT WINAPI FixtureWaveReset(HWAVEIN handle) {
    Require(handle == wave.handle, "WaveIn reset owns its handle");
    for (unsigned int index = 0; index < wave.headers.size(); ++index) {
        if (wave.queued[index]) {
            WAVEHDR* header = wave.headers[index];
            wave.queued[index] = false;
            header->dwFlags = WHDR_PREPARED | WHDR_DONE;
            header->dwBytesRecorded = 0;
            wave.callback(handle, WIM_DATA, wave.instance, reinterpret_cast<DWORD_PTR>(header), 0);
        }
    }
    return MMSYSERR_NOERROR;
}

MMRESULT WINAPI FixtureWaveClose(HWAVEIN handle) {
    Require(handle == wave.handle, "WaveIn close owns its handle");
    ++wave.closes;
    return MMSYSERR_NOERROR;
}

MMRESULT WINAPI FixtureWaveError(MMRESULT, char* message, UINT size) {
    if (size != 0) {
        message[0] = '\0';
    }
    return MMSYSERR_NOERROR;
}

static_assert(std::is_same<decltype(&FixtureWaveOpen), decltype(&waveInOpen)>::value, "real SDK WaveIn signature");
static_assert(std::is_same<decltype(&FixtureWavePrepare), decltype(&waveInPrepareHeader)>::value,
    "real SDK prepare signature");
static_assert(std::is_same<decltype(&FixtureWaveAdd), decltype(&waveInAddBuffer)>::value, "real SDK submit signature");
static_assert(std::is_same<decltype(&FixtureWaveStart), decltype(&waveInStart)>::value, "real SDK start signature");
static_assert(std::is_same<decltype(&FixtureWaveReset), decltype(&waveInReset)>::value, "real SDK reset signature");
static_assert(std::is_same<decltype(&FixtureWaveClose), decltype(&waveInClose)>::value, "real SDK close signature");
static_assert(std::is_same<decltype(&FixtureWaveError), decltype(&waveInGetErrorTextA)>::value,
    "real SDK error signature");

class CaptureBuffer final : public IDirectSoundCaptureBuffer, public IDirectSoundNotify {
public:
    std::vector<unsigned char> bytes;
    WAVEFORMATEX format = {};
    std::uint64_t totalBytes = 0;
    bool running = false;
    bool wrapEvent = false;
    unsigned int starts = 0;
    unsigned int releases = 0;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID identifier, void** output) override {
        if (IsEqualGUID(identifier, IID_IDirectSoundNotify)) {
            *output = static_cast<IDirectSoundNotify*>(this);
            return S_OK;
        }
        *output = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return 1;
    }
    ULONG STDMETHODCALLTYPE Release() override {
        ++releases;
        return 1;
    }
    HRESULT STDMETHODCALLTYPE GetCaps(DSCBCAPS* caps) override {
        caps->dwBufferBytes = static_cast<DWORD>(bytes.size());
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetCurrentPosition(DWORD* capture, DWORD* read) override {
        *capture = static_cast<DWORD>(totalBytes % bytes.size());
        *read = *capture;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetFormat(WAVEFORMATEX*, DWORD, DWORD*) override {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetStatus(DWORD* status) override {
        *status = running ? DSCBSTATUS_CAPTURING : 0;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Initialize(IDirectSoundCapture*, const DSCBUFFERDESC*) override {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE Lock(DWORD start, DWORD count, void** first, DWORD* firstBytes,
        void** second, DWORD* secondBytes, DWORD flags) override {
        Require(flags == 0 && start < bytes.size() && count <= bytes.size(), "bounded actual DSound lock request");
        *firstBytes = std::min(count, static_cast<DWORD>(bytes.size()) - start);
        *secondBytes = count - *firstBytes;
        *first = bytes.data() + start;
        *second = bytes.data();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Start(DWORD flags) override {
        Require(flags == DSCBSTART_LOOPING, "actual DSound loops from initialization");
        running = true;
        ++starts;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Stop() override {
        running = false;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Unlock(void*, DWORD, void*, DWORD) override {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetNotificationPositions(DWORD count, const DSBPOSITIONNOTIFY* notify) override {
        Require(count == 1 && notify->dwOffset == bytes.size() - 1, "actual DSound wrap notification");
        return S_OK;
    }
};

CaptureBuffer captureBuffer;

class CaptureDevice final : public IDirectSoundCapture {
public:
    unsigned int releases = 0;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override {
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return 1;
    }
    ULONG STDMETHODCALLTYPE Release() override {
        ++releases;
        return 1;
    }
    HRESULT STDMETHODCALLTYPE CreateCaptureBuffer(const DSCBUFFERDESC* description,
        IDirectSoundCaptureBuffer** output, IUnknown* outer) override {
        Require(outer == nullptr, "actual DSound creates an unaggregated capture buffer");
        captureBuffer.bytes.assign(description->dwBufferBytes, 0);
        captureBuffer.format = *description->lpwfxFormat;
        captureBuffer.totalBytes = 0;
        captureBuffer.wrapEvent = false;
        captureBuffer.starts = 0;
        captureBuffer.releases = 0;
        captureBuffer.running = false;
        *output = &captureBuffer;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetCaps(DSCCAPS*) override {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE Initialize(const GUID*) override {
        return E_NOTIMPL;
    }
};

CaptureDevice captureDevice;
unsigned int wrongEventDeletes = 0;
unsigned int moduleCloses = 0;
int fakeEvent = 0;

HRESULT WINAPI FixtureCaptureCreate(const GUID* device, IDirectSoundCapture** output, IUnknown* outer) {
    Require(device == nullptr && outer == nullptr, "actual DSound default capture device");
    *output = &captureDevice;
    return S_OK;
}

void* FixtureLoadLibrary(const char* name) {
    Require(std::strcmp(name, "dsound.dll") == 0, "actual DSound module name");
    return &captureDevice;
}

FARPROC WINAPI FixtureGetProcAddress(HMODULE module, const char* name) {
    Require(module == reinterpret_cast<HMODULE>(&captureDevice)
            && std::strcmp(name, "DirectSoundCaptureCreate") == 0,
        "actual DSound resolves its capture entry");
    return reinterpret_cast<FARPROC>(FixtureCaptureCreate);
}

BOOL WINAPI FixtureFreeLibrary(HMODULE module) {
    Require(module == reinterpret_cast<HMODULE>(&captureDevice), "DSound closes its module handle");
    ++moduleCloses;
    return TRUE;
}

HANDLE WINAPI FixtureCreateEvent(SECURITY_ATTRIBUTES*, BOOL manual, BOOL initial, const char* name) {
    Require(!manual && !initial && name == nullptr, "actual DSound auto-reset wrap event");
    return &fakeEvent;
}

DWORD WINAPI FixtureWait(HANDLE event, DWORD timeout) {
    Require(event == &fakeEvent && timeout == 0, "actual DSound polls wrap event without waiting");
    const bool signaled = captureBuffer.wrapEvent;
    captureBuffer.wrapEvent = false;
    return signaled ? WAIT_OBJECT_0 : WAIT_TIMEOUT;
}

BOOL WINAPI FixtureDeleteObject(HGDIOBJ event) {
    Require(event == reinterpret_cast<HGDIOBJ>(&fakeEvent), "legacy source passes its event to DeleteObject");
    ++wrongEventDeletes;
    // Actual DeleteObject does not close an event handle. This fake owns no OS handle.
    return FALSE;
}

}

#define waveInOpen FixtureWaveOpen
#define waveInPrepareHeader FixtureWavePrepare
#define waveInAddBuffer FixtureWaveAdd
#define waveInStart FixtureWaveStart
#define waveInReset FixtureWaveReset
#define waveInClose FixtureWaveClose
#define waveInGetErrorTextA FixtureWaveError
#include CSE_VOICE_WAVEIN_SOURCE
#undef waveInOpen
#undef waveInPrepareHeader
#undef waveInAddBuffer
#undef waveInStart
#undef waveInReset
#undef waveInClose
#undef waveInGetErrorTextA

#define FS_LoadLibrary FixtureLoadLibrary
#define GetProcAddress FixtureGetProcAddress
#define FreeLibrary FixtureFreeLibrary
#define CreateEventA FixtureCreateEvent
#define WaitForSingleObject FixtureWait
#define DeleteObject FixtureDeleteObject
#include CSE_VOICE_DSOUND_SOURCE
#undef FS_LoadLibrary
#undef GetProcAddress
#undef FreeLibrary
#undef CreateEventA
#undef WaitForSingleObject
#undef DeleteObject

#include CSE_VOICE_CIRCULAR_SOURCE

void Con_Printf(const char*, ...) {
}

void Con_DPrintf(const char*, ...) {
}

namespace {

void CheckFormat(const WAVEFORMATEX& format) {
    Require(format.wFormatTag == WAVE_FORMAT_PCM && format.nChannels == 1
            && format.nSamplesPerSec == captureRate && format.nAvgBytesPerSec == captureRate * sampleBytes
            && format.nBlockAlign == sampleBytes && format.wBitsPerSample == sampleBytes * CHAR_BIT
            && format.cbSize == sizeof(WAVEFORMATEX),
        "actual legacy format is mono signed16 at11025Hz including retained cbSize");
}

short Pattern(std::uint64_t sample) {
    constexpr unsigned int positiveSampleMask = 0x7FFF;
    return static_cast<short>(sample & positiveSampleMask);
}

void CompleteWave(unsigned int index, unsigned int firstSample, unsigned int samples) {
    Require(index < wave.headers.size() && wave.queued[index]
            && samples * sampleBytes <= waveBufferBytes,
        "controlled callback only completes a submitted WaveIn header");
    WAVEHDR* header = wave.headers[index];
    for (unsigned int sample = 0; sample < samples; ++sample) {
        const short value = Pattern(firstSample + sample);
        std::memcpy(header->lpData + sample * sampleBytes, &value, sampleBytes);
    }
    wave.queued[index] = false;
    header->dwFlags = WHDR_DONE | WHDR_PREPARED;
    header->dwBytesRecorded = samples * sampleBytes;
    wave.callback(wave.handle, WIM_DATA, wave.instance, reinterpret_cast<DWORD_PTR>(header), 0);
}

void AdvanceDSound(unsigned int bytes) {
    Require(bytes % sampleBytes == 0, "controlled DSound input is whole PCM samples");
    const std::uint64_t previous = captureBuffer.totalBytes;
    const std::uint64_t next = previous + bytes;
    for (std::uint64_t position = previous; position < next; position += sampleBytes) {
        const short value = Pattern(position / sampleBytes);
        std::memcpy(captureBuffer.bytes.data() + position % captureBuffer.bytes.size(), &value, sampleBytes);
    }
    if (next / captureBuffer.bytes.size() != previous / captureBuffer.bytes.size()) {
        captureBuffer.wrapEvent = true;
    }
    captureBuffer.totalBytes = next;
}

void CheckSamples(const short* output, int count, unsigned int first) {
    for (int index = 0; index < count; ++index) {
        Require(output[index] == Pattern(first + static_cast<unsigned int>(index)),
            "captured reference samples retain exact ordering and values");
    }
}

void WaveReference() {
    IVoiceRecord* record = CreateVoiceRecord_WaveIn(captureRate);
    Require(record != nullptr && wave.starts == 1 && wave.prepared == waveBufferCount,
        "WaveIn initializes and starts the device before RecordStart");
    Require(sizeof(void*) != 8 || static_cast<std::uint64_t>(wave.instance) > UINT32_MAX,
        "real 64-bit instance pointer crosses the callback boundary above4GiB");
    CheckFormat(wave.format);
    Require(std::count(wave.queued.begin(), wave.queued.end(), true) == 0,
        "WaveIn initialization submits no headers until RecordStart");
    std::array<short, 4096> output = {};
    Require(record->RecordStart(), "WaveIn RecordStart succeeds");
    constexpr unsigned int samplesPerBuffer = waveBufferBytes / sampleBytes;
    for (unsigned int index = 0; index < waveBufferCount; ++index) {
        CompleteWave(index, index * samplesPerBuffer, samplesPerBuffer);
    }
    int count = record->GetRecordedData(output.data(), defaultCallerSampleCapacity);
    Require(count == defaultCallerSampleCapacity, "WaveIn caller limit counts samples");
    CheckSamples(output.data(), count, waveBufferCount * samplesPerBuffer - defaultCallerSampleCapacity);
    std::printf("wave,caller1024,returned%d,first%d,last%d\n", count, output[0], output[count - 1]);
    CompleteWave(0, 2000, 64);
    record->RecordStop();
    count = record->GetRecordedData(output.data(), 64);
    Require(count == 64 && output[0] == 2000, "WaveIn stop leaves buffered data until read or stopped Idle");
    CompleteWave(1, 3000, 64);
    record->Idle();
    Require(record->RecordStart() && record->GetRecordedData(output.data(), 64) == 0,
        "WaveIn stopped callbacks are discarded and stopped Idle flushes its queue");
    CompleteWave(0, 4000, 64);
    record->RecordStop();
    Require(record->RecordStart() && record->GetRecordedData(output.data(), 64) == 64 && output[0] == 4000,
        "WaveIn restart before stopped Idle retains previously buffered data");
    CompleteWave(0, 5000, 64);
    output.fill(-1);
    Require(record->GetRecordedData(output.data(), 0) == 0 && output[0] == -1
            && record->GetRecordedData(output.data(), 64) == 0,
        "WaveIn zero capacity consumes pending data without writing output");
    std::printf("wave,lifecycle,start_calls%u,instance_high%d\n", wave.starts,
        sizeof(void*) == 8 && static_cast<std::uint64_t>(wave.instance) > UINT32_MAX ? 1 : 0);
    record->Release();
    Require(wave.closes == 1, "WaveIn Release resets and closes its own fake device");
}

void DSoundReference() {
    IVoiceRecord* record = CreateVoiceRecord_DSound(captureRate);
    Require(record != nullptr && captureBuffer.starts == 1 && captureBuffer.running,
        "DSound capture begins in Init");
    CheckFormat(captureBuffer.format);
    Require(captureBuffer.bytes.size() == captureRate * sampleBytes, "DSound allocates one second of PCM");
    std::array<short, 4096> output = {};
    AdvanceDSound(waveBufferCount * waveBufferBytes);
    int count = record->GetRecordedData(output.data(), defaultCallerSampleCapacity);
    Require(count == defaultCallerSampleCapacity / sampleBytes, "DSound caller limit counts bytes");
    CheckSamples(output.data(), count, (waveBufferCount * waveBufferBytes - defaultCallerSampleCapacity) / sampleBytes);
    std::printf("dsound,caller1024,returned%d,first%d,last%d\n", count, output[0], output[count - 1]);
    Require(record->GetRecordedData(output.data(), defaultCallerSampleCapacity) == 0,
        "DSound never returns previously consumed bytes");
    record->RecordStop();
    AdvanceDSound(128);
    count = record->GetRecordedData(output.data(), 128);
    Require(count == 64 && captureBuffer.running && captureBuffer.starts == 1,
        "DSound RecordStop is a no-op and capture remains readable");
    Require(record->RecordStart() && captureBuffer.starts == 1, "DSound RecordStart is a no-op");
    AdvanceDSound(128);
    output.fill(-1);
    Require(record->GetRecordedData(output.data(), 0) == 0 && output[0] == -1
            && record->GetRecordedData(output.data(), 128) == 0,
        "DSound zero capacity advances its last-read position without output");
    AdvanceDSound(captureRate * sampleBytes);
    count = record->GetRecordedData(output.data(), defaultCallerSampleCapacity);
    Require(count == defaultCallerSampleCapacity / sampleBytes, "DSound handles its observed single wrap");
    const auto first = static_cast<unsigned int>((captureBuffer.totalBytes - defaultCallerSampleCapacity) / sampleBytes);
    CheckSamples(output.data(), count, first);
    std::printf("dsound,lifecycle,start_calls%u,buffer_bytes%zu\n", captureBuffer.starts, captureBuffer.bytes.size());
    const unsigned int releasesBefore = captureBuffer.releases;
    record->Release();
    Require(captureBuffer.releases == releasesBefore + 1 && moduleCloses == 1 && wrongEventDeletes == 1,
        "actual DSound teardown releases objects/module and exposes the legacy event-delete mismatch");
}

void DSoundPartialByteReference() {
    IVoiceRecord* record = CreateVoiceRecord_DSound(captureRate);
    Require(record != nullptr, "DSound partial-byte reference initializes");
    std::array<short, 4> output;
    std::memset(output.data(), 0x7E, sizeof(output));
    AdvanceDSound(6);
    Require(record->GetRecordedData(output.data(), 3) == 1, "odd DSound byte limit returns floor(bytes/2)");
    const auto* bytes = reinterpret_cast<const unsigned char*>(output.data());
    Require(bytes[0] == 0 && bytes[1] == 2 && bytes[2] == 0 && bytes[3] == 0x7E,
        "nonwrapped odd DSound request writes its three raw bytes");
    std::memset(output.data(), 0x7E, sizeof(output));
    AdvanceDSound(captureRate * sampleBytes - 4);
    Require(record->GetRecordedData(output.data(), 3) == 1, "wrapped odd DSound byte limit returns one sample");
    const short last = Pattern(captureRate);
    Require(std::memcmp(output.data(), &last, sizeof(last)) == 0 && bytes[2] == 0x7E,
        "legacy second span uses a short-pointer offset and overwrites an odd first-span byte");
    std::puts("dsound,odd_limit3,nonwrapped_writes3,wrapped_writes2,returned1,legacy_overlap_observed");
    record->Release();
}

}

int main() {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#if defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    std::printf("pointer_bytes%zu,wave_header_bytes%zu,format_bytes%zu\n",
        sizeof(void*), sizeof(WAVEHDR), sizeof(WAVEFORMATEX));
    WaveReference();
    DSoundReference();
    DSoundPartialByteReference();
    std::puts("legacy source references passed; no multimedia provider or capture device executed");
    return 0;
}
