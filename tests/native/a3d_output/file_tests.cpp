#include "native_platform.h"
#include "ia3dapi.h"
#include <SDL3/SDL.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <unistd.h>
#include <fcntl.h>
#include <vector>
#define CHECK(value) do { if (!(value)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #value); std::exit(1); } } while (0)
static int OpenDescriptors() {
    int count = 0;
    constexpr int kProbeDescriptors = 1024;
    for (int descriptor = 0; descriptor < kProbeDescriptors; ++descriptor) {
        if (fcntl(descriptor, F_GETFD) >= 0) {
            ++count;
        }
    }
    return count;
}
static std::vector<BYTE> Wave() {
    return {'R','I','F','F',40,0,0,0,'W','A','V','E', 'f','m','t',' ',16,0,0,0,
        1,0,1,0,0x22,0x56,0,0,0x44,0xac,0,0,2,0,16,0, 'd','a','t','a',4,0,0,0,1,2,3,4};
}
static DWORD RunWorker(void* context) {
    auto* worker = static_cast<A3dNativeWorker*>(context);
    while (worker->Wait(1)) {}
    return 0;
}
int main(int argc, char** argv) {
    CHECK(argc == 2);
    const char* test = argv[1];
    if (!std::strcmp(test, "monotonic")) {
        const DWORD start = A3dMonotonicMilliseconds();
        SDL_Delay(5);
        CHECK(DWORD(A3dMonotonicMilliseconds() - start) >= 4);
        CHECK(DWORD(DWORD(2) - DWORD(0xfffffffeu)) == 4);
        return 0;
    }
    if (!std::strcmp(test, "allocation")) {
        void* data = A3dAllocate(16);
        CHECK(data);
        std::memset(data, 42, 16);
        A3dFree(data);
        A3dFree(nullptr);
        return 0;
    }
    if (!std::strcmp(test, "worker")) {
        A3dNativeWorker worker;
        CHECK(worker.Start(RunWorker, &worker) == S_OK);
        CHECK(FAILED(worker.Start(RunWorker, &worker)));
        worker.Stop();
        worker.Stop();
        CHECK(worker.Start(RunWorker, &worker) == S_OK);
        return 0;
    }
    A3dNativeWave wave(!std::strcmp(test, "wave_allocation_failure") ?
        +[](std::size_t) -> void* { return nullptr; } : A3dAllocate);
    if (!std::strcmp(test, "missing_file")) {
        CHECK(wave.Load("/no/such/a3d-fixture.wav") == A3DERROR_FAILED_FILE_OPEN);
        CHECK(!wave.Data() && !wave.Size());
        return 0;
    }
    auto bytes = Wave();
    bool valid = false;
    if (!std::strcmp(test, "short_header")) {
        bytes.resize(7);
    } else if (!std::strcmp(test, "short_data")) {
        bytes.pop_back();
    } else if (!std::strcmp(test, "riff_overflow")) {
        bytes[4] = bytes[5] = bytes[6] = bytes[7] = 255;
    } else if (!std::strcmp(test, "invalid_format")) {
        bytes[32] = 0;
    } else if (!std::strcmp(test, "padded_chunk")) {
        const BYTE chunk[] = {'J','U','N','K',1,0,0,0,99,0};
        bytes.insert(bytes.begin() + 36, std::begin(chunk), std::end(chunk));
        bytes[4] = 50;
        valid = true;
    } else {
        CHECK(!std::strcmp(test, "valid_wave") || !std::strcmp(test, "wave_allocation_failure"));
        valid = true;
    }
    char path[] = "/tmp/cse-a3d-wave-XXXXXX";
    int descriptor = mkstemp(path);
    CHECK(descriptor >= 0);
    CHECK(write(descriptor, bytes.data(), bytes.size()) == static_cast<ssize_t>(bytes.size()));
    CHECK(close(descriptor) == 0);
    const int descriptors = OpenDescriptors();
    const HRESULT result = wave.Load(path);
    CHECK(OpenDescriptors() == descriptors);
    CHECK(unlink(path) == 0);
    if (!std::strcmp(test, "wave_allocation_failure")) {
        CHECK(result == A3DERROR_FAILED_ALLOCATE_WAVEDATA);
        CHECK(!wave.Data() && !wave.Size());
        return 0;
    }
    if (valid) {
        CHECK(result == S_OK);
        CHECK(wave.Size() == 4 && wave.Data()[3] == 4);
        CHECK(wave.Format().wf.nSamplesPerSec == 22050);
        CHECK(wave.Format().wBitsPerSample == 16);
        CHECK(wave.Load("/no/such/a3d-fixture.wav") == A3DERROR_FAILED_FILE_OPEN);
    } else {
        CHECK(result == A3DERROR_UNRECOGNIZED_FORMAT);
    }
    CHECK(!wave.Data() && !wave.Size());
}
