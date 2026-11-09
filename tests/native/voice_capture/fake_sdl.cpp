#include "fake_sdl.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <thread>
#include <vector>

struct SDL_AudioStream {
    std::recursive_mutex lock;
    std::mutex deviceLock;
    SDL_AudioStreamCallback callback = nullptr;
    void* userdata = nullptr;
    SDL_AudioSpec format = {};
    std::vector<unsigned char> input;
    std::size_t offset = 0;
    bool paused = true;
    unsigned int id = 0;
    std::thread worker;
};

namespace {

struct Allocation {
    void* address;
    unsigned int streamId;
};

std::mutex registryLock;
std::vector<SDL_AudioStream*> streams;
std::vector<Allocation> allocations;
std::vector<unsigned int> destroyedStreams;
unsigned int nextStreamId = 1;
std::atomic<fake_sdl::Failure> failure{fake_sdl::Failure::None};
std::atomic<unsigned int> allocationCount{0};
std::atomic<unsigned int> freeCount{0};
std::atomic<unsigned int> initCount{0};
std::atomic<unsigned int> quitCount{0};
std::atomic<unsigned int> references{0};
std::atomic<unsigned int> openCount{0};
std::atomic<unsigned int> resumeCount{0};
std::atomic<unsigned int> pauseCount{0};
std::atomic<unsigned int> detachCount{0};
std::atomic<unsigned int> destroyCount{0};
std::atomic<unsigned int> callbackCount{0};
std::atomic<unsigned int> activeCallbacks{0};
std::atomic<unsigned int> callbackViolations{0};
std::atomic<unsigned int> diagnostics{0};
std::atomic<unsigned int> readCalls{0};
std::atomic<unsigned int> invalidCalls{0};
std::atomic<std::uint64_t> allocatedBytes{0};
std::atomic<SDL_AudioFormat> requestedFormat{SDL_AUDIO_UNKNOWN};
std::atomic<int> requestedChannels{0};
std::atomic<int> requestedRate{0};
std::atomic<int> modulesAlive{0};
std::atomic<bool> overrideAvailable{false};
std::atomic<int> availableValue{0};
thread_local bool insideCallback = false;

std::mutex lifecycleLock;
std::condition_variable lifecycleWake;

std::mutex readGateLock;
std::condition_variable readGateWake;
bool blockRead = false;
bool readBlocked = false;
bool releaseRead = false;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "fake SDL boundary: %s\n", message);
        std::abort();
    }
}

void OwnerOperation() {
    if (insideCallback) {
        ++callbackViolations;
    }
}

bool Alive(SDL_AudioStream* stream) {
    std::lock_guard<std::mutex> guard(registryLock);
    if (std::find(streams.begin(), streams.end(), stream) != streams.end()) {
        return true;
    }
    ++invalidCalls;
    return false;
}

bool ConsumeFailure(fake_sdl::Failure expected) {
    return failure.compare_exchange_strong(expected, fake_sdl::Failure::None);
}

void PushLocked(SDL_AudioStream* stream, const void* data, std::size_t bytes, int additionalHint, int totalHint) {
    std::lock_guard<std::recursive_mutex> guard(stream->lock);
    if (stream->offset != 0) {
        stream->input.erase(stream->input.begin(), stream->input.begin() + stream->offset);
        stream->offset = 0;
    }
    const auto* input = static_cast<const unsigned char*>(data);
    stream->input.insert(stream->input.end(), input, input + bytes);
    if (stream->paused || stream->callback == nullptr) {
        return;
    }
    ++callbackCount;
    ++activeCallbacks;
    insideCallback = true;
    stream->callback(stream->userdata, stream, additionalHint, totalHint);
    insideCallback = false;
    --activeCallbacks;
}

}

namespace fake_sdl {

void Reset() {
    std::lock_guard<std::mutex> guard(registryLock);
    Require(streams.empty() && allocations.empty() && activeCallbacks == 0 && references == 0,
        "test must release its actual component/resources before resetting the fake boundary");
    destroyedStreams.clear();
    nextStreamId = 1;
    failure = Failure::None;
    allocationCount = 0;
    freeCount = 0;
    initCount = 0;
    quitCount = 0;
    openCount = 0;
    resumeCount = 0;
    pauseCount = 0;
    detachCount = 0;
    destroyCount = 0;
    callbackCount = 0;
    callbackViolations = 0;
    diagnostics = 0;
    readCalls = 0;
    invalidCalls = 0;
    allocatedBytes = 0;
    requestedFormat = SDL_AUDIO_UNKNOWN;
    requestedChannels = 0;
    requestedRate = 0;
    overrideAvailable = false;
    {
        std::lock_guard<std::mutex> gate(readGateLock);
        blockRead = false;
        readBlocked = false;
        releaseRead = false;
    }
}

void Fail(Failure value) {
    failure = value;
}

Stats Observe() {
    std::lock_guard<std::mutex> guard(registryLock);
    return {allocationCount, freeCount, initCount, quitCount, references, openCount, resumeCount,
        pauseCount, detachCount, destroyCount, static_cast<unsigned int>(streams.size()), callbackCount,
        activeCallbacks, callbackViolations, diagnostics, readCalls, invalidCalls, allocatedBytes,
        requestedFormat, requestedChannels, requestedRate};
}

SDL_AudioStream* LastStream() {
    std::lock_guard<std::mutex> guard(registryLock);
    return streams.empty() ? nullptr : streams.back();
}

SDL_AudioStreamCallback CallbackFunction(SDL_AudioStream* stream) {
    Require(Alive(stream), "callback observation targets a live stream");
    std::lock_guard<std::recursive_mutex> guard(stream->lock);
    return stream->callback;
}

void Push(SDL_AudioStream* stream, const void* data, std::size_t bytes, int additionalHint, int totalHint) {
    Require(Alive(stream), "control push targets a live stream");
    std::lock_guard<std::mutex> device(stream->deviceLock);
    PushLocked(stream, data, bytes, additionalHint, totalHint);
}

void BlockNextRead() {
    std::lock_guard<std::mutex> guard(readGateLock);
    blockRead = true;
    readBlocked = false;
    releaseRead = false;
}

bool WaitForBlockedRead(unsigned int milliseconds) {
    std::unique_lock<std::mutex> guard(readGateLock);
    return readGateWake.wait_for(guard, std::chrono::milliseconds(milliseconds), [] { return readBlocked; });
}

void UnblockRead() {
    std::lock_guard<std::mutex> guard(readGateLock);
    releaseRead = true;
    readGateWake.notify_all();
}

void PushFromWorker(SDL_AudioStream* stream, const void* data, std::size_t bytes) {
    Require(Alive(stream) && !stream->worker.joinable(), "one controlled worker per stream");
    std::vector<unsigned char> copy(static_cast<const unsigned char*>(data),
        static_cast<const unsigned char*>(data) + bytes);
    stream->worker = std::thread([stream, copy] {
        Push(stream, copy.data(), copy.size());
    });
}

void JoinWorker(SDL_AudioStream* stream) {
    Require(Alive(stream), "join targets a live stream");
    if (stream->worker.joinable()) {
        stream->worker.join();
    }
}

void NextAvailable(int bytes) {
    availableValue = bytes;
    overrideAvailable = true;
}

std::size_t QueuedBytes(SDL_AudioStream* stream) {
    Require(Alive(stream), "queue observation targets a live stream");
    std::lock_guard<std::recursive_mutex> guard(stream->lock);
    return stream->input.size() - stream->offset;
}

bool WaitForPauseAttempt(unsigned int milliseconds) {
    std::unique_lock<std::mutex> guard(lifecycleLock);
    return lifecycleWake.wait_for(guard, std::chrono::milliseconds(milliseconds), [] { return pauseCount != 0; });
}

void HoldExternalAudioReference() {
    ++references;
}

void ReleaseExternalAudioReference() {
    Require(references != 0, "external audio reference exists");
    --references;
}

bool CallbackActive() {
    return activeCallbacks != 0;
}

bool InsideCallback() {
    return insideCallback;
}

void NoteModuleLoaded(bool loaded) {
    if (loaded) {
        ++modulesAlive;
    } else {
        --modulesAlive;
    }
}

int ModulesAlive() {
    return modulesAlive;
}

}

extern "C" void* SDLCALL SDL_malloc(std::size_t bytes) {
    OwnerOperation();
    allocatedBytes = bytes;
    constexpr std::size_t maximumFixtureAllocation = 1024 * 1024;
    if (ConsumeFailure(fake_sdl::Failure::Allocation) || bytes > maximumFixtureAllocation) {
        return nullptr;
    }
    void* memory = std::malloc(bytes);
    if (memory != nullptr) {
        std::lock_guard<std::mutex> guard(registryLock);
        allocations.push_back({memory, 0});
        ++allocationCount;
    }
    return memory;
}

extern "C" void SDLCALL SDL_free(void* memory) {
    OwnerOperation();
    if (memory == nullptr) {
        return;
    }
    {
        std::lock_guard<std::mutex> guard(registryLock);
        const auto allocation = std::find_if(allocations.begin(), allocations.end(),
            [memory](const Allocation& value) { return value.address == memory; });
        Require(allocation != allocations.end(), "component frees only its acquired queue allocation");
        Require(allocation->streamId == 0 || std::find(destroyedStreams.begin(), destroyedStreams.end(),
                allocation->streamId) != destroyedStreams.end(),
            "component destroys/quiesces its stream before freeing callback queue storage");
        allocations.erase(allocation);
    }
    ++freeCount;
    std::free(memory);
}

extern "C" bool SDLCALL SDL_InitSubSystem(SDL_InitFlags flags) {
    OwnerOperation();
    Require(flags == SDL_INIT_AUDIO, "component acquires only the audio subsystem");
    ++initCount;
    if (ConsumeFailure(fake_sdl::Failure::Init)) {
        return false;
    }
    ++references;
    return true;
}

extern "C" void SDLCALL SDL_QuitSubSystem(SDL_InitFlags flags) {
    OwnerOperation();
    Require(flags == SDL_INIT_AUDIO && references != 0, "component releases an acquired audio reference");
    if (references == 1) {
        std::lock_guard<std::mutex> guard(registryLock);
        Require(streams.empty(), "last audio quit would destroy live streams; owner must close them first");
    }
    ++quitCount;
    --references;
}

extern "C" SDL_AudioStream* SDLCALL SDL_OpenAudioDeviceStream(SDL_AudioDeviceID device, const SDL_AudioSpec* format,
    SDL_AudioStreamCallback callback, void* userdata) {
    OwnerOperation();
    Require(references != 0 && device == SDL_AUDIO_DEVICE_DEFAULT_RECORDING && format != nullptr,
        "component opens a recording stream with a supplied app format");
    ++openCount;
    requestedFormat = format->format;
    requestedChannels = format->channels;
    requestedRate = format->freq;
    if (ConsumeFailure(fake_sdl::Failure::Open)) {
        return nullptr;
    }
    auto* stream = new SDL_AudioStream;
    stream->format = *format;
    stream->callback = callback;
    stream->userdata = userdata;
    std::lock_guard<std::mutex> guard(registryLock);
    stream->id = nextStreamId++;
    streams.push_back(stream);
    const auto allocation = std::find_if(allocations.rbegin(), allocations.rend(),
        [](const Allocation& value) { return value.streamId == 0; });
    Require(allocation != allocations.rend(), "component queue exists before stream callback registration");
    allocation->streamId = stream->id;
    return stream;
}

extern "C" bool SDLCALL SDL_GetAudioStreamFormat(SDL_AudioStream* stream, SDL_AudioSpec* input,
    SDL_AudioSpec* output) {
    if (!Alive(stream) || ConsumeFailure(fake_sdl::Failure::FormatQuery)) {
        return false;
    }
    std::lock_guard<std::recursive_mutex> guard(stream->lock);
    if (input != nullptr) {
        *input = stream->format;
    }
    if (output != nullptr) {
        *output = stream->format;
        if (ConsumeFailure(fake_sdl::Failure::WrongFormat)) {
            output->format = SDL_AUDIO_F32;
        } else if (ConsumeFailure(fake_sdl::Failure::WrongChannels)) {
            output->channels = 2;
        } else if (ConsumeFailure(fake_sdl::Failure::WrongRate)) {
            ++output->freq;
        }
    }
    return true;
}

extern "C" bool SDLCALL SDL_ResumeAudioStreamDevice(SDL_AudioStream* stream) {
    OwnerOperation();
    if (!Alive(stream)) {
        return false;
    }
    std::lock_guard<std::mutex> guard(stream->deviceLock);
    ++resumeCount;
    if (ConsumeFailure(fake_sdl::Failure::Resume)) {
        return false;
    }
    stream->paused = false;
    return true;
}

extern "C" bool SDLCALL SDL_PauseAudioStreamDevice(SDL_AudioStream* stream) {
    OwnerOperation();
    if (!Alive(stream)) {
        return false;
    }
    ++pauseCount;
    lifecycleWake.notify_all();
    std::lock_guard<std::mutex> guard(stream->deviceLock);
    if (ConsumeFailure(fake_sdl::Failure::Pause)) {
        return false;
    }
    stream->paused = true;
    return true;
}

extern "C" bool SDLCALL SDL_SetAudioStreamPutCallback(SDL_AudioStream* stream, SDL_AudioStreamCallback callback,
    void* userdata) {
    OwnerOperation();
    if (!Alive(stream)) {
        return false;
    }
    std::lock_guard<std::recursive_mutex> guard(stream->lock);
    stream->callback = callback;
    stream->userdata = userdata;
    if (callback == nullptr) {
        ++detachCount;
    }
    return true;
}

extern "C" bool SDLCALL SDL_LockAudioStream(SDL_AudioStream* stream) {
    if (!Alive(stream)) {
        return false;
    }
    stream->lock.lock();
    return true;
}

extern "C" bool SDLCALL SDL_UnlockAudioStream(SDL_AudioStream* stream) {
    if (!Alive(stream)) {
        return false;
    }
    stream->lock.unlock();
    return true;
}

extern "C" int SDLCALL SDL_GetAudioStreamAvailable(SDL_AudioStream* stream) {
    if (!Alive(stream) || ConsumeFailure(fake_sdl::Failure::Available)) {
        return -1;
    }
    std::lock_guard<std::recursive_mutex> guard(stream->lock);
    if (overrideAvailable.exchange(false)) {
        return availableValue;
    }
    const std::size_t bytes = stream->input.size() - stream->offset;
    return static_cast<int>(std::min(bytes, static_cast<std::size_t>(std::numeric_limits<int>::max())));
}

extern "C" bool SDLCALL SDL_ClearAudioStream(SDL_AudioStream* stream) {
    if (!Alive(stream)) {
        return false;
    }
    std::lock_guard<std::recursive_mutex> guard(stream->lock);
    stream->input.clear();
    stream->offset = 0;
    return true;
}

extern "C" int SDLCALL SDL_GetAudioStreamData(SDL_AudioStream* stream, void* output, int bytes) {
    if (!Alive(stream) || output == nullptr || bytes < 0) {
        return -1;
    }
    std::lock_guard<std::recursive_mutex> guard(stream->lock);
    ++readCalls;
    {
        std::unique_lock<std::mutex> gate(readGateLock);
        if (blockRead) {
            blockRead = false;
            readBlocked = true;
            readGateWake.notify_all();
            readGateWake.wait(gate, [] { return releaseRead; });
        }
    }
    if (ConsumeFailure(fake_sdl::Failure::Read)) {
        return -1;
    }
    if (ConsumeFailure(fake_sdl::Failure::ReadZero)) {
        return 0;
    }
    if (ConsumeFailure(fake_sdl::Failure::ReadOversized)) {
        return bytes + 2;
    }
    int count = static_cast<int>(std::min(static_cast<std::size_t>(bytes), stream->input.size() - stream->offset));
    if (ConsumeFailure(fake_sdl::Failure::ReadOdd)) {
        count = 1;
    }
    if (count != 0) {
        std::memcpy(output, stream->input.data() + stream->offset, static_cast<std::size_t>(count));
        stream->offset += static_cast<std::size_t>(count);
    }
    return count;
}

extern "C" void SDLCALL SDL_DestroyAudioStream(SDL_AudioStream* stream) {
    OwnerOperation();
    Require(Alive(stream), "destroy targets a live component stream");
    {
        std::lock_guard<std::mutex> device(stream->deviceLock);
        std::lock_guard<std::recursive_mutex> guard(stream->lock);
        Require(stream->callback == nullptr && stream->userdata == nullptr,
            "component detaches userdata before stream destruction");
        stream->paused = true;
    }
    if (stream->worker.joinable()) {
        stream->worker.join();
    }
    {
        std::lock_guard<std::mutex> guard(registryLock);
        streams.erase(std::find(streams.begin(), streams.end(), stream));
        destroyedStreams.push_back(stream->id);
    }
    ++destroyCount;
    delete stream;
}

void Con_DPrintf(const char*, ...) {
    if (insideCallback) {
        ++callbackViolations;
    }
    ++diagnostics;
}
