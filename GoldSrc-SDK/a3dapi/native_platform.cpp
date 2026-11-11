#include "native_platform.h"
#include "ia3dapi.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

std::uint32_t A3dMonotonicMilliseconds() noexcept {
    const auto elapsed = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<std::uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
}

void* A3dAllocate(std::size_t size) noexcept {
    return std::malloc(size);
}

void A3dFree(void* data) noexcept {
    std::free(data);
}

A3dNativeWorker::~A3dNativeWorker() {
    Stop();
}

int A3dNativeWorker::Run(void* context) {
    auto* worker = static_cast<A3dNativeWorker*>(context);
    worker->function_(worker->context_);
    return 0;
}

HRESULT A3dNativeWorker::Start(Function function, void* context) noexcept {
    if (thread_ || !function) {
        return E_INVALIDARG;
    }
    mutex_ = SDL_CreateMutex();
    condition_ = SDL_CreateCondition();
    if (!mutex_ || !condition_) {
        Stop();
        return E_OUTOFMEMORY;
    }
    function_ = function;
    context_ = context;
    stopping_ = false;
    thread_ = SDL_CreateThread(Run, "A3D worker", this);
    if (!thread_) {
        Stop();
        return E_FAIL;
    }
    return S_OK;
}

bool A3dNativeWorker::Wait(DWORD milliseconds) noexcept {
    SDL_LockMutex(mutex_);
    if (!stopping_) {
        SDL_WaitConditionTimeout(condition_, mutex_, milliseconds);
    }
    const bool running = !stopping_;
    SDL_UnlockMutex(mutex_);
    return running;
}

void A3dNativeWorker::Stop() noexcept {
    if (thread_) {
        SDL_LockMutex(mutex_);
        stopping_ = true;
        SDL_BroadcastCondition(condition_);
        SDL_UnlockMutex(mutex_);
        SDL_WaitThread(thread_, nullptr);
        thread_ = nullptr;
    }
    SDL_DestroyCondition(condition_);
    SDL_DestroyMutex(mutex_);
    condition_ = nullptr;
    mutex_ = nullptr;
    function_ = nullptr;
    context_ = nullptr;
}

namespace {
class WaveFile {
public:
    explicit WaveFile(const char* name) : file_(name ? std::fopen(name, "rb") : nullptr) {}
    ~WaveFile() {
        if (file_) {
            std::fclose(file_);
        }
    }
    std::FILE* Get() const {
        return file_;
    }
    bool Read(void* data, std::size_t size) {
        return std::fread(data, 1, size, file_) == size;
    }
    bool Seek(std::uint64_t position) {
        return position <= static_cast<std::uint64_t>(std::numeric_limits<off_t>::max()) &&
            fseeko(file_, static_cast<off_t>(position), SEEK_SET) == 0;
    }
private:
    std::FILE* file_;
};

WORD Little16(const BYTE* data) {
    constexpr unsigned kBitsPerByte = 8;
    return static_cast<WORD>(data[0] | (static_cast<WORD>(data[1]) << kBitsPerByte));
}

DWORD Little32(const BYTE* data) {
    constexpr unsigned kBitsPerByte = 8;
    DWORD result = 0;
    for (std::size_t index = 0; index < sizeof(result); ++index) {
        result |= static_cast<DWORD>(data[index]) << (index * kBitsPerByte);
    }
    return result;
}
}

A3dNativeWave::~A3dNativeWave() {
    free_(data_);
}

HRESULT A3dNativeWave::Load(const char* filename) noexcept {
    free_(data_);
    data_ = nullptr;
    size_ = 0;
    format_ = {};
    WaveFile file(filename);
    if (!file.Get()) {
        return A3DERROR_FAILED_FILE_OPEN;
    }
    constexpr std::size_t kRiffHeaderBytes = 12;
    constexpr std::size_t kChunkHeaderBytes = 8;
    constexpr std::size_t kPcmFormatBytes = 16;
    constexpr std::size_t kFourCcBytes = 4;
    BYTE header[kRiffHeaderBytes];
    if (!file.Read(header, sizeof(header)) || std::memcmp(header, "RIFF", kFourCcBytes) ||
        std::memcmp(header + kChunkHeaderBytes, "WAVE", kFourCcBytes) || fseeko(file.Get(), 0, SEEK_END)) {
        return A3DERROR_UNRECOGNIZED_FORMAT;
    }
    const off_t fileBytes = ftello(file.Get());
    const std::uint64_t riffEnd = static_cast<std::uint64_t>(Little32(header + kFourCcBytes)) + kChunkHeaderBytes;
    if (fileBytes < 0 || riffEnd < kRiffHeaderBytes || riffEnd > static_cast<std::uint64_t>(fileBytes)) {
        return A3DERROR_UNRECOGNIZED_FORMAT;
    }
    PCMWAVEFORMAT format{};
    bool haveFormat = false;
    std::uint64_t position = kRiffHeaderBytes;
    while (position <= riffEnd && riffEnd - position >= kChunkHeaderBytes) {
        BYTE chunk[kChunkHeaderBytes];
        if (!file.Seek(position) || !file.Read(chunk, sizeof(chunk))) {
            return A3DERROR_UNRECOGNIZED_FORMAT;
        }
        const DWORD chunkBytes = Little32(chunk + kFourCcBytes);
        position += kChunkHeaderBytes;
        if (chunkBytes > riffEnd - position) {
            return A3DERROR_UNRECOGNIZED_FORMAT;
        }
        if (!std::memcmp(chunk, "fmt ", kFourCcBytes)) {
            BYTE pcm[kPcmFormatBytes];
            if (haveFormat || chunkBytes < sizeof(pcm) || !file.Read(pcm, sizeof(pcm))) {
                return A3DERROR_UNRECOGNIZED_FORMAT;
            }
            // PCM fmt chunk field offsets, in bytes from the beginning of the payload.
            constexpr std::size_t kChannelsOffset = 2;
            constexpr std::size_t kSampleRateOffset = 4;
            constexpr std::size_t kByteRateOffset = 8;
            constexpr std::size_t kBlockAlignOffset = 12;
            constexpr std::size_t kSampleBitsOffset = 14;
            format.wf.wFormatTag = Little16(pcm);
            format.wf.nChannels = Little16(pcm + kChannelsOffset);
            format.wf.nSamplesPerSec = Little32(pcm + kSampleRateOffset);
            format.wf.nAvgBytesPerSec = Little32(pcm + kByteRateOffset);
            format.wf.nBlockAlign = Little16(pcm + kBlockAlignOffset);
            format.wBitsPerSample = Little16(pcm + kSampleBitsOffset);
            constexpr WORD kPcmTag = 1;
            constexpr WORD kByteBits = 8;
            constexpr WORD kPcm16Bits = 16;
            constexpr WORD kStereoChannels = 2;
            if (format.wf.wFormatTag != kPcmTag || !format.wf.nChannels ||
                format.wf.nChannels > kStereoChannels || !format.wf.nSamplesPerSec ||
                (format.wBitsPerSample != kByteBits && format.wBitsPerSample != kPcm16Bits) ||
                format.wf.nBlockAlign != format.wf.nChannels * (format.wBitsPerSample / kByteBits) ||
                static_cast<std::uint64_t>(format.wf.nSamplesPerSec) * format.wf.nBlockAlign !=
                    format.wf.nAvgBytesPerSec) {
                return A3DERROR_UNRECOGNIZED_FORMAT;
            }
            haveFormat = true;
        } else if (!std::memcmp(chunk, "data", kFourCcBytes)) {
            if (!haveFormat || !chunkBytes || chunkBytes > static_cast<DWORD>(std::numeric_limits<INT>::max()) ||
                chunkBytes % format.wf.nBlockAlign) {
                return A3DERROR_UNRECOGNIZED_FORMAT;
            }
            auto* data = static_cast<BYTE*>(allocate_(chunkBytes));
            if (!data) {
                return A3DERROR_FAILED_ALLOCATE_WAVEDATA;
            }
            if (!file.Read(data, chunkBytes)) {
                free_(data);
                return A3DERROR_UNRECOGNIZED_FORMAT;
            }
            data_ = data;
            size_ = chunkBytes;
            format_ = format;
            return S_OK;
        }
        // RIFF chunks are word aligned, including an odd-sized chunk's padding byte.
        position += static_cast<std::uint64_t>(chunkBytes) + (chunkBytes & 1u);
    }
    return A3DERROR_UNRECOGNIZED_FORMAT;
}
